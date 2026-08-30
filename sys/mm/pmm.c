#include <stddef.h>
#include <stdint.h>

#include <boot_info.h>
#include <memset.h>

#include "pmm.h"

struct free_page {
    struct free_page *next;
};

static struct free_page *free_list;
static uint64_t          free_page_count;
static uint64_t          total_pages;

void pmm_init(const struct memmap_entry *memmap, size_t count) {
    free_list       = nullptr;
    free_page_count = 0;
    total_pages     = 0;

    for (size_t i = 0; i < count; i++) {
        if (memmap[i].type != MEM_USABLE) {
            continue;
        }
        uintptr_t base = (memmap[i].base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uintptr_t end  = (memmap[i].base + memmap[i].size) & ~(PAGE_SIZE - 1);

        for (uintptr_t addr = base; addr < end; addr += PAGE_SIZE) {
            if (addr == 0) {
                continue; // never hand out the null page
            }
            struct free_page *page = (struct free_page *)addr;
            page->next             = free_list;
            free_list              = page;
            free_page_count++;
            total_pages++;
        }
    }
}

uintptr_t pmm_alloc_page(void) {
    if (!free_list) {
        return 0; // out of physical memory
    }

    struct free_page *page = free_list;
    free_list              = page->next;
    free_page_count--;

    uintptr_t addr = (uintptr_t)page;
    memset((void *)addr, 0, PAGE_SIZE);
    return addr;
}

void pmm_free_page(uintptr_t paddr) {
    struct free_page *page = (struct free_page *)paddr;
    page->next             = free_list;
    free_list              = page;
    free_page_count++;
}

size_t pmm_free_pages(void) { return free_page_count; }
size_t pmm_total_pages(void) { return total_pages; }
