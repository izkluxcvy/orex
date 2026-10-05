#pragma once

#include <stdint.h>

#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_DS   (0x18 | 3)
#define USER_CS   (0x20 | 3)
#define TSS_SEL   0x28

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

struct tss {
    uint32_t reserved0;
    uint64_t rsp0; // kernel stack loaded on a ring 3 -> ring 0 transition
    uint64_t rsp1, rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

struct cpu;

void gdt_init();
void gdt_init_cpu(struct cpu *c);
void cpu_set_gs(struct cpu *c);
void tss_set_rsp0(uint64_t rsp0);
