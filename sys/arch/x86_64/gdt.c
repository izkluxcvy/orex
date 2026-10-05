#include <stdint.h>

#include <cpu.h>

#include "msr.h"
#include "segment.h"

static struct segment_descriptor gdts[MAX_CPUS][7];

static struct tss tsss[MAX_CPUS];

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

static void set_tss(struct segment_descriptor *desc, struct tss *tss) {
    uint64_t base  = (uint64_t)tss;
    uint64_t limit = sizeof(*tss) - 1;

    *desc = (struct segment_descriptor){
        .limit_low  = limit & 0xffff,
        .base_low   = base & 0xffff,
        .base_mid   = (base >> 16) & 0xff,
        .type       = 0x9, // Available 64-bit TSS
        .present    = 1,
        .limit_high = (limit >> 16) & 0xf,
        .base_high  = (base >> 24) & 0xff,
    };
    *(uint64_t *)(desc + 1) = base >> 32;
}

void tss_set_rsp0(uint64_t rsp0) { tsss[curcpu()->id].rsp0 = rsp0; }

void cpu_set_gs(struct cpu *c) {
    c->self = c;
    wrmsr(MSR_GS_BASE, (uint64_t)c);
}

void gdt_init_cpu(struct cpu *c) {
    struct segment_descriptor *gdt = gdts[c->id];
    gdt[0] = (struct segment_descriptor){0}; // Null descriptor
    set_kcode(&gdt[1]);
    set_kdata(&gdt[2]);
    set_kdata(&gdt[3]); // User data segment
    gdt[3].dpl = 3;
    set_kcode(&gdt[4]); // User code segment
    gdt[4].dpl = 3;

    tsss[c->id].iomap_base = sizeof(struct tss);
    set_tss(&gdt[5], &tsss[c->id]);

    load_gdt(gdt);
    reload_segments();
    cpu_set_gs(c);
    __asm__ __volatile__("ltr %0" : : "r"((uint16_t)TSS_SEL));
}

void gdt_init() { gdt_init_cpu(&cpus[0]); }
