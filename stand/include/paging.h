#pragma once

#include <stdint.h>

#define PAGE_POOL_PAGES 64

struct mem_range {
    uintptr_t base;
    size_t    size;
};

struct page_pool {
    uintptr_t base;
    size_t    size;
    int       used;
};
