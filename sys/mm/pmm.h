#pragma once

#include <stddef.h>
#include <stdint.h>

#include <boot_info.h>

#define PAGE_SIZE 0x1000ULL

void      pmm_init(const struct memmap_entry *memmap, size_t count);
uintptr_t pmm_alloc_page(void);
void      pmm_free_page(uintptr_t paddr);
size_t    pmm_free_pages(void);
size_t    pmm_total_pages(void);
