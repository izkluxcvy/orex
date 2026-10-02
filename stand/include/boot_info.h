#pragma once

#include <stddef.h>
#include <stdint.h>

struct framebuffer {
    uintptr_t base;
    size_t    size, width, height, pixels_per_scanline, pixel_format;
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
    struct framebuffer fb;

    struct memmap_entry *memmap;
    size_t               memmap_count;

    uintptr_t kernel_phys_base;
    uintptr_t kernel_virt_base;
    size_t    kernel_size;

    uintptr_t acpi_rsdp;

    char cmdline[256];

    uintptr_t initrd_base;
    size_t    initrd_size;
};
