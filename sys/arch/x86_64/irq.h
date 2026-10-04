#pragma once

#include <stdint.h>

#include "trap.h"

#define IRQ_VECTOR_BASE     0x20
#define IRQ_VECTOR_TIMER    0x20
#define IRQ_VECTOR_SPURIOUS 0xFF

static inline uint64_t irq_save() {
    uint64_t flags;
    __asm__ __volatile__("pushfq\n\t"
                         "pop %0\n\t"
                         "cli"
                         : "=r"(flags)
                         :
                         : "memory");
    return flags;
}

static inline void irq_enable() { __asm__ __volatile__("sti"); }

static inline void irq_restore(uint64_t flags) {
    __asm__ __volatile__("push %0\n\t"
                         "popfq"
                         :
                         : "r"(flags)
                         : "memory", "cc");
}

typedef void (*irq_handler_t)(struct trapframe *tf);

void irq_init();
void irq_register(uint8_t vector, irq_handler_t handler);
void irq_dispatch(struct trapframe *tf);
