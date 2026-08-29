#pragma once

#include <stdint.h>

#define KERNEL_CS 0x08
#define KERNEL_DS 0x10

struct segment_descriptor {
    uint64_t limit_low   : 16;
    uint64_t base_low    : 16;
    uint64_t base_mid    : 8;
    uint64_t type        : 4;
    uint64_t system      : 1;
    uint64_t dpl         : 2;
    uint64_t present     : 1;
    uint64_t limit_high  : 4;
    uint64_t available   : 1;
    uint64_t long_mode   : 1;
    uint64_t def_opsz    : 1;
    uint64_t granularity : 1;
    uint64_t base_high   : 8;
} __attribute__((packed));
