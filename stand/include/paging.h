#pragma once

#include <stddef.h>
#include <stdint.h>

struct mem_range {
    uintptr_t base;
    size_t    size;
};

struct page_pool {
    uintptr_t base;
    size_t    size;
    size_t    used;
};

uint64_t paging_init(struct page_pool *pool, const struct mem_range *ranges,
                     size_t range_count, uint64_t kernel_phys_base,
                     uint64_t kernel_virt_base, uint64_t kernel_size);
