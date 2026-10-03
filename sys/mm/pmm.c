#include <stddef.h>
#include <stdint.h>

#include <boot_info.h>
#include <memset.h>

#include "pmap.h"
#include "pmm.h"

static uintptr_t free_list;
static uint64_t  free_page_count;
static uint64_t  total_pages;
static uintptr_t pages_pa;
static uint64_t  npages;

static inline uintptr_t *link_of(uintptr_t pa) { return phys_to_virt(pa); }

struct page *pmm_page(uintptr_t pa) {
    uint64_t pfn = pa / PAGE_SIZE;
    return pfn < npages ? (struct page *)phys_to_virt(pages_pa) + pfn : nullptr;
}

void pmm_init(const struct memmap_entry *memmap, size_t count) {
    free_list       = 0;
    free_page_count = 0;
    total_pages     = 0;

    for (size_t i = 0; i < count; i++) {
        if (memmap[i].type == MEM_USABLE) {
            uint64_t end = (memmap[i].base + memmap[i].size) / PAGE_SIZE;
            npages       = end > npages ? end : npages;
        }
    }

    size_t need =
        (npages * sizeof(struct page) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uintptr_t taken_lo = 0, taken_hi = 0;
    for (size_t i = 0; i < count && !pages_pa; i++) {
        uintptr_t base = (memmap[i].base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uintptr_t end  = (memmap[i].base + memmap[i].size) & ~(PAGE_SIZE - 1);
        if (memmap[i].type == MEM_USABLE && base && end > base &&
            end - base >= need) {
            pages_pa = base;
            taken_lo = base;
            taken_hi = base + need;
        }
    }
    memset(phys_to_virt(pages_pa), 0, need);

    for (size_t i = 0; i < count; i++) {
        if (memmap[i].type != MEM_USABLE) {
            continue;
        }
        uintptr_t base = (memmap[i].base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uintptr_t end  = (memmap[i].base + memmap[i].size) & ~(PAGE_SIZE - 1);

        for (uintptr_t pa = base; pa < end; pa += PAGE_SIZE) {
            if (pa == 0 || (pa >= taken_lo && pa < taken_hi)) {
                continue;
            }
            *link_of(pa) = free_list;
            free_list    = pa;
            free_page_count++;
            total_pages++;
        }
    }
}

uintptr_t pmm_alloc_page_dirty() {
    uintptr_t pa = free_list;
    if (pa) {
        free_list = *link_of(pa);
        free_page_count--;
        *pmm_page(pa) = (struct page){.refs = 1};
    }
    return pa;
}

uintptr_t pmm_alloc_page() {
    uintptr_t pa = pmm_alloc_page_dirty();
    if (pa) {
        memset(phys_to_virt(pa), 0, PAGE_SIZE);
    }
    return pa;
}

void pmm_free_page(uintptr_t pa) {
    struct page *pg = pmm_page(pa);
    if (pg) {
        *pg = (struct page){.refs = 0};
    }
    *link_of(pa) = free_list;
    free_list    = pa;
    free_page_count++;
}

size_t pmm_free_pages() { return free_page_count; }
size_t pmm_total_pages() { return total_pages; }

void pmm_page_ref(uintptr_t pa) {
    struct page *pg = pmm_page(pa);
    if (pg) {
        __atomic_add_fetch(&pg->refs, 1, __ATOMIC_RELAXED);
    }
}

void pmm_page_unref(uintptr_t pa) {
    struct page *pg = pmm_page(pa);
    if (!pg || __atomic_sub_fetch(&pg->refs, 1, __ATOMIC_ACQ_REL) == 0) {
        pmm_free_page(pa);
    }
}

uint32_t pmm_page_refs(uintptr_t pa) {
    struct page *pg = pmm_page(pa);
    return pg ? __atomic_load_n(&pg->refs, __ATOMIC_RELAXED) : 1;
}
