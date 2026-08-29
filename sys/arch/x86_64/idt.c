#include <stdint.h>

#include <printf.h>

#include "segment.h"
#include "serial.h"

struct interrupt_descriptor {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct interrupt_frame {
    uint64_t ip;
    uint64_t cs;
    uint64_t flags;
    uint64_t sp;
    uint64_t ss;
};

#define IDT_ENTRIES    256
#define GATE_INTERRUPT 0x8e // present, ring 0, 64-bit interrupt gate

static struct interrupt_descriptor idt[IDT_ENTRIES];

static const char *const exception_names[32] = {
    "Divide Error",
    "Debug",
    "NMI Interrupt",
    "Breakpoint",
    "Overflow",
    "BOUND Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved",
};

static void halt(void) {
    __asm__ __volatile__("cli");
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

static void dump_frame(uint8_t vector, uint64_t error_code,
                       struct interrupt_frame *frame) {
    printf("\n*** CPU exception: %s", exception_names[vector]);
    printf(" (vector %x", vector);
    printf(", error %lx", error_code);
    printf(")\nrip=%lx", frame->ip);
    printf(" cs=%lx", frame->cs);
    printf(" flags=%lx", frame->flags);
    printf("\nrsp=%lx", frame->sp);
    printf(" ss=%lx", frame->ss);

    if (vector == 14) { // Page Fault: CR2 holds the faulting address
        uint64_t cr2;
        __asm__ __volatile__("mov %0, cr2" : "=r"(cr2));
        printf("\ncr2=%lx", cr2);
    }

    printf("\n");
}

static void exception_common(uint8_t vector, uint64_t error_code,
                             struct interrupt_frame *frame) {
    dump_frame(vector, error_code, frame);
    halt();
}

#define DEFINE_ISR_NOERR(vec)                                                  \
    __attribute__((interrupt)) static void isr##vec(                           \
        struct interrupt_frame *frame) {                                       \
        exception_common(vec, 0, frame);                                       \
    }

#define DEFINE_ISR_ERR(vec)                                                    \
    __attribute__((interrupt)) static void isr##vec(                           \
        struct interrupt_frame *frame, unsigned long long error_code) {        \
        exception_common(vec, error_code, frame);                              \
    }

DEFINE_ISR_NOERR(0)
DEFINE_ISR_NOERR(1)
DEFINE_ISR_NOERR(2)
DEFINE_ISR_NOERR(3)
DEFINE_ISR_NOERR(4)
DEFINE_ISR_NOERR(5)
DEFINE_ISR_NOERR(6)
DEFINE_ISR_NOERR(7)
DEFINE_ISR_ERR(8)
DEFINE_ISR_NOERR(9)
DEFINE_ISR_ERR(10)
DEFINE_ISR_ERR(11)
DEFINE_ISR_ERR(12)
DEFINE_ISR_ERR(13)
DEFINE_ISR_ERR(14)
DEFINE_ISR_NOERR(15)
DEFINE_ISR_NOERR(16)
DEFINE_ISR_ERR(17)
DEFINE_ISR_NOERR(18)
DEFINE_ISR_NOERR(19)
DEFINE_ISR_NOERR(20)
DEFINE_ISR_ERR(21)
DEFINE_ISR_NOERR(22)
DEFINE_ISR_NOERR(23)
DEFINE_ISR_NOERR(24)
DEFINE_ISR_NOERR(25)
DEFINE_ISR_NOERR(26)
DEFINE_ISR_NOERR(27)
DEFINE_ISR_NOERR(28)
DEFINE_ISR_ERR(29)
DEFINE_ISR_ERR(30)
DEFINE_ISR_NOERR(31)

static void *const isr_table[32] = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,  isr8,  isr9,  isr10,
    isr11, isr12, isr13, isr14, isr15, isr16, isr17, isr18, isr19, isr20, isr21,
    isr22, isr23, isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
};

static void set_gate(uint8_t vector, void *handler, uint8_t type_attr) {
    uint64_t addr = (uint64_t)handler;

    idt[vector].offset_low  = addr & 0xffff;
    idt[vector].selector    = KERNEL_CS;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (addr >> 16) & 0xffff;
    idt[vector].offset_high = (addr >> 32) & 0xffffffff;
    idt[vector].reserved    = 0;
}

static void load_idt(void) {
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idtr = {.limit = sizeof(idt) - 1,
                                      .base  = (uint64_t)&idt};

    __asm__ __volatile__("lidt %0" : : "m"(idtr));
}

void idt_init(void) {
    for (int vector = 0; vector < 32; vector++) {
        set_gate(vector, isr_table[vector], GATE_INTERRUPT);
    }
    load_idt();
}
