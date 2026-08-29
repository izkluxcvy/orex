#include <stddef.h>
#include <stdint.h>

#include <boot_info.h>
#include <memset.h>

#include "pmm.h"

static uint8_t *bitmap;
static uint64_t total_pages;
static uint64_t free_page_count;
static uint64_t search_cursor;

static inline int bit_test(uint64_t page) {
    return (bitmap[page / 8] >> (page % 8)) & 1;
}

static inline void bit_set(uint64_t page) {
    bitmap[page / 8] |= (uint8_t)(1u << (page % 8));
}

static inline void bit_clear(uint64_t page) {
    bitmap[page / 8] &= (uint8_t)~(1u << (page % 8));
}

static void mark_used(uint64_t page, uint64_t count) {
    for (uint64_t i = 0; i < count && page + i < total_pages; i++) {
        if (!bit_test(page + i)) {
            bit_set(page + i);
            free_page_count--;
        }
    }
}

static void mark_free(uint64_t page, uint64_t count) {
    for (uint64_t i = 0; i < count && page + i < total_pages; i++) {
        if (bit_test(page + i)) {
            bit_clear(page + i);
            free_page_count++;
        }
    }
}

void pmm_init(const struct memmap_entry *memmap, size_t count) {
    uint64_t max_addr = 0;
    for (size_t i = 0; i < count; i++) {
        if (memmap[i].type != MEM_USABLE) {
            continue;
        }
        uint64_t end = memmap[i].base + memmap[i].size;
        if (end > max_addr) {
            max_addr = end;
        }
    }

    total_pages            = (max_addr + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t    bitmap_bytes = (size_t)((total_pages + 7) / 8);
    size_t    bitmap_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t bitmap_addr  = 0;

    for (size_t i = 0; i < count; i++) {
        if (memmap[i].type != MEM_USABLE) {
            continue;
        }
        uintptr_t base = (memmap[i].base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uintptr_t end  = (memmap[i].base + memmap[i].size) & ~(PAGE_SIZE - 1);
        if (end > base && (end - base) >= bitmap_pages * PAGE_SIZE) {
            bitmap_addr = base;
            break;
        }
    }

    if (bitmap_addr == 0) {
        __builtin_trap(); // no usable range large enough to host the bitmap
    }

    bitmap = (uint8_t *)bitmap_addr;
    memset(bitmap, 0xff,
           bitmap_bytes); // everything reserved until proven usable
    free_page_count = 0;

    for (size_t i = 0; i < count; i++) {
        if (memmap[i].type != MEM_USABLE) {
            continue;
        }
        uintptr_t base = (memmap[i].base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uintptr_t end  = (memmap[i].base + memmap[i].size) & ~(PAGE_SIZE - 1);
        if (end <= base) {
            continue;
        }
        mark_free(base / PAGE_SIZE, (end - base) / PAGE_SIZE);
    }

    mark_used(bitmap_addr / PAGE_SIZE, bitmap_pages);
    mark_used(0, 1); // never hand out the null page

    search_cursor = 0;
}

uintptr_t pmm_alloc_page(void) {
    for (uint64_t i = 0; i < total_pages; i++) {
        uint64_t page = (search_cursor + i) % total_pages;
        if (!bit_test(page)) {
            bit_set(page);
            free_page_count--;
            search_cursor  = page + 1;
            uintptr_t addr = (uintptr_t)(page * PAGE_SIZE);
            memset((void *)addr, 0, PAGE_SIZE);
            return addr;
        }
    }
    return 0; // out of physical memory
}

void pmm_free_page(uintptr_t paddr) { mark_free(paddr / PAGE_SIZE, 1); }

size_t pmm_free_pages(void) { return free_page_count; }
size_t pmm_total_pages(void) { return total_pages; }
