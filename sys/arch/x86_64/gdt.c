#include <stdint.h>

#include "segment.h"

struct segment_descriptor gdt[3];

static void set_kcode(struct segment_descriptor *desc) {
    desc->type        = 0xA; // Code segment
    desc->system      = 1;   // Code/data segment, Execute/Read
    desc->dpl         = 0;   // Kernel level
    desc->present     = 1;
    desc->available   = 0;
    desc->long_mode   = 1; // 64-bit code segment
    desc->def_opsz    = 0; // 64-bit code segment
    desc->granularity = 1; // 4KB granularity
}

static void set_kdata(struct segment_descriptor *desc) {
    set_kcode(desc);
    desc->type      = 0x2; // Data segment, Read/Write
    desc->long_mode = 0;   // Not a code segment
    desc->def_opsz  = 1;   // Not a code segment
}

static void load_gdt() {
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr = {.limit = sizeof(gdt) - 1,
                                      .base  = (uint64_t)&gdt};

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

void gdt_init() {
    gdt[0] = (struct segment_descriptor){0}; // Null descriptor
    set_kcode(&gdt[1]);                      // Kernel code segment
    set_kdata(&gdt[2]);                      // Kernel data segment
    load_gdt();
    reload_segments();
}
