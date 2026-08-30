#include <stddef.h>
#include <stdint.h>

#include "pmap.h"
#include "pmm.h"
#include <kmalloc.h>

#define KHEAP_BASE     0xffffffffc0000000ULL
#define KHEAP_LIMIT    0xffffffffe0000000ULL // 512 MiB arena ceiling
#define KHEAP_GROW_MIN (16 * PAGE_SIZE)      // grow in >=64K steps
#define ALIGN          16

struct block {
    size_t        size; // usable bytes following this header
    int           free;
    struct block *next;
    struct block *prev;
};

static struct block *head;
static struct block *tail;
static uintptr_t     heap_end; // next unmapped address in the arena

void kmalloc_init(void) {
    head     = nullptr;
    tail     = nullptr;
    heap_end = KHEAP_BASE;
}

static int heap_grow(size_t min_bytes) {
    size_t grow_bytes = min_bytes > KHEAP_GROW_MIN ? min_bytes : KHEAP_GROW_MIN;
    grow_bytes        = (grow_bytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    if (heap_end + grow_bytes > KHEAP_LIMIT) {
        grow_bytes = KHEAP_LIMIT - heap_end;
        if (grow_bytes < min_bytes) {
            return -1; // heap arena exhausted
        }
    }

    uintptr_t start = heap_end;
    for (uintptr_t v = start; v < start + grow_bytes; v += PAGE_SIZE) {
        uintptr_t page = pmm_alloc_page();
        if (page == 0) {
            return -1; // out of physical memory
        }
        if (pmap_kenter(v, page, PMAP_PRESENT | PMAP_WRITE) != 0) {
            pmm_free_page(page);
            return -1;
        }
    }
    heap_end += grow_bytes;

    if (tail && tail->free) {
        tail->size += grow_bytes;
    } else {
        struct block *b = (struct block *)start;
        b->size         = grow_bytes - sizeof(struct block);
        b->free         = 1;
        b->next         = nullptr;
        b->prev         = tail;
        if (tail) {
            tail->next = b;
        }
        tail = b;
        if (!head) {
            head = b;
        }
    }
    return 0;
}

void *kmalloc(size_t size) {
    if (size == 0) {
        return nullptr;
    }
    size = (size + (ALIGN - 1)) & ~(size_t)(ALIGN - 1);

    for (;;) {
        for (struct block *b = head; b; b = b->next) {
            if (!b->free || b->size < size) {
                continue;
            }

            size_t remaining = b->size - size;
            if (remaining >= sizeof(struct block) + ALIGN) {
                struct block *split =
                    (struct block *)((uint8_t *)(b + 1) + size);
                split->size = remaining - sizeof(struct block);
                split->free = 1;
                split->next = b->next;
                split->prev = b;
                if (b->next) {
                    b->next->prev = split;
                }
                b->next = split;
                b->size = size;
                if (tail == b) {
                    tail = split;
                }
            }

            b->free = 0;
            return (void *)(b + 1);
        }

        // Nothing free was big enough; grow the arena and retry once.
        if (heap_grow(size + sizeof(struct block)) != 0) {
            return nullptr;
        }
    }
}

void kfree(void *ptr) {
    if (!ptr) {
        return;
    }
    struct block *b = (struct block *)ptr - 1;
    b->free         = 1;

    if (b->next && b->next->free) {
        struct block *n = b->next;
        b->size += sizeof(struct block) + n->size;
        b->next = n->next;
        if (n->next) {
            n->next->prev = b;
        }
        if (tail == n) {
            tail = b;
        }
    }

    if (b->prev && b->prev->free) {
        struct block *p = b->prev;
        p->size += sizeof(struct block) + b->size;
        p->next = b->next;
        if (b->next) {
            b->next->prev = p;
        }
        if (tail == b) {
            tail = p;
        }
    }
}
