#include <stdint.h>

#include <cpu.h>

#include "msr.h"
#include "segment.h"

static struct segment_descriptor gdts[MAX_CPUS][7];

static void set_kcode(struct segment_descriptor *desc) {
    desc->type        = 0xa; // Code segment
    desc->system      = 1;   // Code/data segment, Execute/Read
    desc->dpl         = 0;   // Ring 0
    desc->present     = 1;
    desc->available   = 0;
    desc->long_mode   = 1;
    desc->def_opsz    = 0;
    desc->granularity = 1; // 4KB granularity
}

static void set_kdata(struct segment_descriptor *desc) {
    set_kcode(desc);
    desc->type      = 0x2; // Data segment, Read/Write
    desc->long_mode = 0;   // Not a code segment
    desc->def_opsz  = 1;
}

static void load_gdt(struct segment_descriptor *gdt) {
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr = {.limit = sizeof(gdts[0]) - 1,
                                      .base  = (uint64_t)gdt};

    __asm__ __volatile__("lgdt %0" : : "m"(gdtr));
}

static void reload_segments() {
    __asm__ __volatile__("push %0\n\t"
                         "lea rax, [rip + 1f]\n\t"
                         "push rax\n\t"
                         "retfq\n\t"
                         "1:\n\t"
                         "mov ax, %1\n\t"
                         "mov ds, ax\n\t"
                         "mov es, ax\n\t"
                         "mov ss, ax\n\t"
                         "mov fs, ax\n\t"
                         "mov gs, ax\n\t"
                         :
                         : "i"(KERNEL_CS), "i"(KERNEL_DS)
                         : "rax", "memory");
}

void cpu_set_gs(struct cpu *c) {
    c->self = c;
    wrmsr(MSR_GS_BASE, (uint64_t)c);
}

void gdt_init_cpu(struct cpu *c) {
    struct segment_descriptor *gdt = gdts[c->id];
    gdt[0] = (struct segment_descriptor){0}; // Null descriptor
    set_kcode(&gdt[1]);
    set_kdata(&gdt[2]);

    load_gdt(gdt);
    reload_segments();
    cpu_set_gs(c);
}

void gdt_init() { gdt_init_cpu(&cpus[0]); }
