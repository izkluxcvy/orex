#pragma once

#include <stddef.h>
#include <stdint.h>

#include <boot_info.h>

#define PAGE_SIZE 0x1000ull

struct page {
    uint32_t     refs;
    uint16_t     slab_class;
    uint16_t     inuse;
    void        *free_list;
    struct page *next;
};

void         pmm_init(const struct memmap_entry *memmap, size_t count);
uintptr_t    pmm_alloc_page();
uintptr_t    pmm_alloc_page_dirty();
void         pmm_free_page(uintptr_t paddr);
size_t       pmm_free_pages();
size_t       pmm_total_pages();
struct page *pmm_page(uintptr_t paddr);
void         pmm_page_ref(uintptr_t paddr);
void         pmm_page_unref(uintptr_t paddr);
uint32_t     pmm_page_refs(uintptr_t paddr);
