#pragma once

#include <stdint.h>

#include <boot_info.h>

extern uintptr_t dmap_offset;

static inline void *phys_to_virt(uintptr_t pa) {
    return (void *)(pa + dmap_offset);
}
