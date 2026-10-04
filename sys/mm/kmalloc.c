#include <stddef.h>
#include <stdint.h>

#include <kmalloc.h>

#include "pmap.h"
#include "pmm.h"

#define KHEAP_BASE  0xFFFF'FFFF'C000'0000ull
#define KHEAP_LIMIT 0xFFFF'FFFF'E000'0000ull // 512 MB
#define MIN_SHIFT   4                        // 16 bytes
#define NCLASS      8                        // 16 .. 2048
#define SMALL_MAX   (1u << (MIN_SHIFT + NCLASS - 1))
#define HDR         16

static struct page *partial[NCLASS];
static int          nempty[NCLASS];

struct range {
    uintptr_t     start, pages;
    struct range *next;
};

static struct range *free_ranges;
static uintptr_t     arena_end = KHEAP_BASE;

static int class_of(size_t size) {
    int c = 0;
    while ((1u << (MIN_SHIFT + c)) < size) {
        c++;
    }
    return c;
}

static uintptr_t page_pa(const struct page *pg) {
    return (uintptr_t)(pg - pmm_page(0)) * PAGE_SIZE;
}

static struct page *slab_new(int c) {
    uintptr_t pa = pmm_alloc_page_dirty();
    if (!pa) {
        return nullptr;
    }

    struct page *pg   = pmm_page(pa);
    size_t       sz   = 1u << (MIN_SHIFT + c);
    uint8_t     *mem  = phys_to_virt(pa);
    void        *head = nullptr;
    for (size_t off = PAGE_SIZE; off >= sz; off -= sz) {
        void **obj = (void **)(mem + off - sz);
        *obj       = head;
        head       = obj;
    }
    *pg = (struct page){
        .refs = 1, .slab_class = (uint16_t)(c + 1), .free_list = head};
    return pg;
}

static void *small_alloc(int c) {
    struct page *pg = partial[c];
    if (!pg) {
        if (!(pg = slab_new(c))) {
            return nullptr;
        }
        pg->next   = nullptr;
        partial[c] = pg;
    } else if (!pg->inuse) {
        nempty[c]--;
    }

    void **obj    = pg->free_list;
    pg->free_list = *obj;
    pg->inuse++;
    if (!pg->free_list) {
        partial[c] = pg->next;
    }
    return obj;
}

static void small_free(struct page *pg, void *p) {
    int    c      = pg->slab_class - 1;
    void **obj    = p;
    int    full   = !pg->free_list;
    *obj          = pg->free_list;
    pg->free_list = obj;
    pg->inuse--;
    if (full) {
        pg->next   = partial[c];
        partial[c] = pg;
    }
    if (pg->inuse) {
        return;
    }
    if (nempty[c] == 0) {
        nempty[c]++;
        return;
    }
    for (struct page **pp = &partial[c]; *pp; pp = &(*pp)->next) {
        if (*pp == pg) {
            *pp = pg->next;
            break;
        }
    }
    pmm_free_page(page_pa(pg));
}

static uintptr_t range_take(uintptr_t pages) {
    for (struct range **pp = &free_ranges; *pp; pp = &(*pp)->next) {
        struct range *r = *pp;
        if (r->pages >= pages) {
            uintptr_t at = r->start;
            r->start += pages * PAGE_SIZE;
            r->pages -= pages;
            if (!r->pages) {
                *pp = r->next;
                small_free(pmm_page(kvirt_to_phys(r)), r);
            }
            return at;
        }
    }

    if (arena_end + pages * PAGE_SIZE > KHEAP_LIMIT) {
        return 0;
    }
    uintptr_t at = arena_end;
    arena_end += pages * PAGE_SIZE;
    return at;
}

static void range_give(uintptr_t start, uintptr_t pages) {
    struct range **pp = &free_ranges, *prev = nullptr;
    while (*pp && (*pp)->start < start) {
        prev = *pp;
        pp   = &(*pp)->next;
    }
    if (prev && prev->start + prev->pages * PAGE_SIZE == start) {
        prev->pages += pages;
        if (*pp && start + pages * PAGE_SIZE == (*pp)->start) {
            struct range *n = *pp;
            prev->pages += n->pages;
            prev->next = n->next;
            small_free(pmm_page(kvirt_to_phys(n)), n);
        }
        return;
    }

    if (*pp && start + pages * PAGE_SIZE == (*pp)->start) {
        (*pp)->start = start;
        (*pp)->pages += pages;
        return;
    }
    struct range *r = small_alloc(class_of(sizeof(*r)));
    if (!r) {
        return;
    }
    *r  = (struct range){.start = start, .pages = pages, .next = *pp};
    *pp = r;
}

static void *big_alloc(size_t size) {
    uintptr_t pages = (size + HDR + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t va    = range_take(pages);
    if (!va) {
        return nullptr;
    }

    for (uintptr_t i = 0; i < pages; i++) {
        uintptr_t pa = pmm_alloc_page_dirty();
        if (!pa || pmap_kenter(va + i * PAGE_SIZE, pa,
                               PMAP_PRESENT | PMAP_WRITE) != 0) {
            if (pa) {
                pmm_free_page(pa);
            }
            while (i--) {
                uintptr_t v = va + i * PAGE_SIZE;
                pmm_free_page(kvirt_to_phys((void *)v));
                pmap_kremove(v);
            }
            range_give(va, pages);
            return nullptr;
        }
    }
    *(uintptr_t *)va = pages;
    return (void *)(va + HDR);
}

static void big_free(void *p) {
    uintptr_t va    = (uintptr_t)p - HDR;
    uintptr_t pages = *(uintptr_t *)va;
    for (uintptr_t i = 0; i < pages; i++) {
        uintptr_t v = va + i * PAGE_SIZE;
        pmm_free_page(kvirt_to_phys((void *)v));
        pmap_kremove(v);
    }
    range_give(va, pages);
}

void *kmalloc(size_t size) {
    if (size == 0) {
        return nullptr;
    }
    void *p = size <= SMALL_MAX ? small_alloc(class_of(size)) : big_alloc(size);
    return p;
}

void kfree(void *ptr) {
    if (!ptr) {
        return;
    }
    if ((uintptr_t)ptr >= KHEAP_BASE && (uintptr_t)ptr < KHEAP_LIMIT) {
        big_free(ptr);
    } else {
        small_free(pmm_page(kvirt_to_phys(ptr)), ptr);
    }
}
