#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pthread_impl.h>
#include <syscall.h>

#define ALIGN      16
#define HDR        16
#define MIN_CHUNK  32
#define SEG_MIN    (1UL << 20)
#define MMAP_LIMIT (256UL << 20)
#define PAGE       4096UL

#define INUSE      1UL
#define PREV_INUSE 2UL
#define MMAPPED    4UL
#define FLAGS      15UL

struct chunk {
    size_t prev_size;
    size_t head;
    struct chunk *next, *prev;
};

struct fence {
    size_t prev_size;
    size_t head;
    size_t seg_size;
    size_t pad;
};

#define SMALL_BINS 64
#define NBINS      (SMALL_BINS + 2 * 54)

static struct chunk *bins[NBINS];
static uint64_t nonempty[(NBINS + 63) / 64];
static int cached_empty_segments;

static size_t size_of(const struct chunk *c) { return c->head & ~FLAGS; }

static struct chunk *at(void *p, size_t off) {
    return (struct chunk *)((char *)p + off);
}

static struct chunk *next_of(struct chunk *c) { return at(c, size_of(c)); }
static void *mem_of(struct chunk *c) { return (char *)c + HDR; }
static struct chunk *chunk_of(void *p) {
    return (struct chunk *)((char *)p - HDR);
}

[[noreturn]] static void corrupt(const char *what) {
    static const char pre[] = "malloc: ";
    write(STDERR_FILENO, pre, sizeof(pre) - 1);
    write(STDERR_FILENO, what, strlen(what));
    write(STDERR_FILENO, "\n", 1);
    abort();
}

static int bin_index(size_t size) {
    if (size < 1024) {
        return (int)(size / ALIGN);
    }
    int msb = 63 - __builtin_clzl(size);
    int i = SMALL_BINS + 2 * (msb - 10) + (int)((size >> (msb - 1)) & 1);
    return i < NBINS ? i : NBINS - 1;
}

static void bin_insert(struct chunk *c) {
    int i = bin_index(size_of(c));
    c->prev = nullptr;
    c->next = bins[i];
    if (bins[i]) {
        bins[i]->prev = c;
    }
    bins[i] = c;
    nonempty[i / 64] |= 1ULL << (i % 64);
}

static void bin_remove(struct chunk *c) {
    int i = bin_index(size_of(c));
    if (c->prev) {
        c->prev->next = c->next;
    } else {
        bins[i] = c->next;
    }
    if (c->next) {
        c->next->prev = c->prev;
    }
    if (!bins[i]) {
        nonempty[i / 64] &= ~(1ULL << (i % 64));
    }
}

static void *map(size_t len) {
    long p = __syscall6(SYS_mmap, 0, (long)len, 3, 0x22, -1, 0);
    return (unsigned long)p > -4096UL ? nullptr : (void *)p;
}

static void unmap(void *p, size_t len) {
    __syscall2(SYS_munmap, (long)p, (long)len);
}

static void mark_free(struct chunk *c, size_t size) {
    c->head = size | (c->head & PREV_INUSE);
    struct chunk *n = at(c, size);
    n->prev_size = size;
    n->head &= ~PREV_INUSE;
}

static void release(struct chunk *c) {
    size_t size = size_of(c);
    if (!(c->head & PREV_INUSE)) {
        struct chunk *p = (struct chunk *)((char *)c - c->prev_size);
        bin_remove(p);
        size += size_of(p);
        c = p;
    }
    struct chunk *n = at(c, size);
    if (!(n->head & INUSE)) {
        bin_remove(n);
        size += size_of(n);
    }
    mark_free(c, size);

    struct fence *f = (struct fence *)at(c, size);
    if (size_of((struct chunk *)f) == 0 &&
        size == f->seg_size - sizeof(struct fence) &&
        cached_empty_segments++ > 0) {
        cached_empty_segments--;
        unmap(c, f->seg_size);
        return;
    }
    bin_insert(c);
}

static void trim(struct chunk *c, size_t need) {
    size_t size = size_of(c);
    if (size - need < MIN_CHUNK) {
        return;
    }
    c->head = need | (c->head & FLAGS);
    struct chunk *r = at(c, need);
    r->head = (size - need) | INUSE | PREV_INUSE;
    at(r, size - need)->head |= PREV_INUSE;
    release(r);
}

static struct chunk *new_segment(size_t need) {
    size_t len = need + sizeof(struct fence) + PAGE - 1;
    len = len < SEG_MIN ? SEG_MIN : len & ~(PAGE - 1);
    struct chunk *c = map(len);
    if (!c) {
        return nullptr;
    }
    size_t size = len - sizeof(struct fence);
    struct fence *f = (struct fence *)at(c, size);
    f->head = INUSE;
    f->seg_size = len;
    c->head = size | PREV_INUSE;
    at(c, size)->prev_size = size;
    return c;
}

static struct chunk *take_free(size_t need) {
    int i = bin_index(need);
    if (i >= SMALL_BINS) {
        for (struct chunk *c = bins[i]; c; c = c->next) {
            if (size_of(c) >= need) {
                bin_remove(c);
                return c;
            }
        }
        i++;
    }
    for (int w = i / 64; w < (int)(sizeof(nonempty) / 8); w++) {
        uint64_t bits = nonempty[w];
        if (w == i / 64) {
            bits &= ~0ULL << (i % 64);
        }
        if (bits) {
            struct chunk *c = bins[w * 64 + __builtin_ctzl(bits)];
            bin_remove(c);
            return c;
        }
    }
    return nullptr;
}

static size_t request(size_t n) {
    size_t need = (n + sizeof(size_t) + ALIGN - 1) & ~(ALIGN - 1);
    return need < MIN_CHUNK ? MIN_CHUNK : need;
}

static void *alloc(size_t n) {
    if (n > SIZE_MAX / 2) {
        errno = ENOMEM;
        return nullptr;
    }
    size_t need = request(n);
    if (need >= MMAP_LIMIT) {
        size_t len = (need + HDR + PAGE - 1) & ~(PAGE - 1);
        struct chunk *c = map(len);
        if (!c) {
            errno = ENOMEM;
            return nullptr;
        }
        c->prev_size = 0;
        c->head = len | MMAPPED | INUSE;
        return mem_of(c);
    }

    struct chunk *c = take_free(need);
    if (!c) {
        if (!(c = new_segment(need))) {
            errno = ENOMEM;
            return nullptr;
        }
    } else if (!size_of(next_of(c)) &&
               size_of(c) + sizeof(struct fence) ==
                   ((struct fence *)next_of(c))->seg_size) {
        cached_empty_segments--;
    }
    c->head |= INUSE;
    next_of(c)->head |= PREV_INUSE;
    trim(c, need);
    return mem_of(c);
}

static volatile int heap_lock;

void __malloc_fork(int phase) { // 0: before, 1: after in parent, 2: child
    if (phase == 0) {
        __lock(&heap_lock);
    } else if (phase == 1) {
        __unlock(&heap_lock);
    } else {
        heap_lock = 0;
    }
}

void *malloc(size_t n) {
    __lock(&heap_lock);
    void *p = alloc(n);
    __unlock(&heap_lock);
    return p;
}

static void free_unlocked(void *p) {
    if (!p) {
        return;
    }
    struct chunk *c = chunk_of(p);
    if (((uintptr_t)p & (ALIGN - 1)) || !(c->head & INUSE)) {
        corrupt("free(): invalid pointer");
    }
    if (c->head & MMAPPED) {
        unmap((char *)c - c->prev_size, size_of(c) + c->prev_size);
        return;
    }
    if (!(next_of(c)->head & PREV_INUSE)) {
        corrupt("free(): double free");
    }
    release(c);
}

void free(void *p) {
    if (p) {
        __lock(&heap_lock);
        free_unlocked(p);
        __unlock(&heap_lock);
    }
}

void *calloc(size_t n, size_t size) {
    size_t total;
    if (__builtin_mul_overflow(n, size, &total)) {
        errno = ENOMEM;
        return nullptr;
    }
    __lock(&heap_lock);
    void *p = alloc(total);
    int map = p && (chunk_of(p)->head & MMAPPED);
    __unlock(&heap_lock);
    if (p && !map) {
        memset(p, 0, total);
    }
    return p;
}

static size_t usable(struct chunk *c) {
    return (c->head & MMAPPED) ? size_of(c) - HDR : size_of(c) - sizeof(size_t);
}

static void *realloc_unlocked(void *p, size_t n) {
    if (!p) {
        return alloc(n);
    }
    if (n > SIZE_MAX / 2) {
        errno = ENOMEM;
        return nullptr;
    }
    struct chunk *c = chunk_of(p);
    size_t need = request(n);
    if (!(c->head & MMAPPED)) {
        struct chunk *nx = next_of(c);
        if (size_of(c) < need && !(nx->head & INUSE) &&
            size_of(c) + size_of(nx) >= need) {
            bin_remove(nx);
            c->head += size_of(nx);
            next_of(c)->head |= PREV_INUSE;
        }
        if (size_of(c) >= need) {
            trim(c, need);
            return p;
        }
    } else if (usable(c) >= n && usable(c) / 2 < n) {
        return p;
    }
    void *q = alloc(n);
    if (q) {
        size_t keep = usable(c);
        memcpy(q, p, keep < n ? keep : n);
        free_unlocked(p);
    }
    return q;
}

void *realloc(void *p, size_t n) {
    __lock(&heap_lock);
    void *q = realloc_unlocked(p, n);
    __unlock(&heap_lock);
    return q;
}

void *reallocarray(void *p, size_t n, size_t size) {
    size_t total;
    if (__builtin_mul_overflow(n, size, &total)) {
        errno = ENOMEM;
        return nullptr;
    }
    return realloc(p, total);
}

static void *alloc_aligned(size_t align, size_t n) {
    if (align < ALIGN) {
        return alloc(n);
    }
    if (n > SIZE_MAX / 2 - align) {
        errno = ENOMEM;
        return nullptr;
    }
    char *p = alloc(n + align + MIN_CHUNK);
    if (!p) {
        return nullptr;
    }
    struct chunk *c = chunk_of(p);
    uintptr_t m = ((uintptr_t)p + align - 1) & ~(align - 1);
    if (m != (uintptr_t)p && m - (uintptr_t)p < MIN_CHUNK) {
        m += align;
    }
    if (m == (uintptr_t)p) {
        if (!(c->head & MMAPPED)) {
            trim(c, request(n));
        }
        return p;
    }
    size_t lead = m - (uintptr_t)p;
    struct chunk *nc = chunk_of((void *)m);
    if (c->head & MMAPPED) {
        nc->prev_size = c->prev_size + lead;
        nc->head = (size_of(c) - lead) | MMAPPED | INUSE;
        return (void *)m;
    }
    nc->head = (size_of(c) - lead) | INUSE | PREV_INUSE;
    c->head = lead | INUSE | (c->head & PREV_INUSE);
    release(c);
    trim(nc, request(n));
    return (void *)m;
}

int posix_memalign(void **out, size_t align, size_t n) {
    if (align < sizeof(void *) || (align & (align - 1))) {
        return EINVAL;
    }
    int saved = errno;
    __lock(&heap_lock);
    void *p = alloc_aligned(align, n);
    __unlock(&heap_lock);
    errno = saved;
    if (!p) {
        return ENOMEM;
    }
    *out = p;
    return 0;
}

void *aligned_alloc(size_t align, size_t n) {
    if (!align || (align & (align - 1))) {
        errno = EINVAL;
        return nullptr;
    }
    __lock(&heap_lock);
    void *p = alloc_aligned(align, n);
    __unlock(&heap_lock);
    return p;
}

void free_sized(void *p, size_t n) {
    (void)n;
    free(p);
}

void free_aligned_sized(void *p, size_t n, size_t align) {
    (void)n, (void)align;
    free(p);
}
