#pragma once

#include <stdint.h>

struct framebuffer {
    uintptr_t base;
    size_t    size;
    size_t    width;
    size_t    height;
    size_t    pixels_per_scanline;
    size_t    pixel_format; // 0: RGB, 1: BGR
};

enum mem_type {
    MEM_USABLE,
    MEM_RESERVED,
};

struct memmap_entry {
    uintptr_t     base;
    size_t        size;
    enum mem_type type;
};

struct boot_info {
    struct framebuffer  *fb;
    struct memmap_entry *memmap;
    size_t               memmap_count;
    uintptr_t            kernel_phys_base;
    uintptr_t            kernel_virt_base;
    size_t               kernel_size;
};
