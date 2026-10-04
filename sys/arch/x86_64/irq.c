#include <stdint.h>

#include <printf.h>

#include "apic.h"
#include "irq.h"
#include "trap.h"

#define IRQ_COUNT (TRAP_VECTORS - IRQ_VECTOR_BASE)

static irq_handler_t handlers[IRQ_COUNT];

void irq_init() {
    for (unsigned i = 0; i < IRQ_COUNT; i++) {
        handlers[i] = nullptr;
    }
}

void irq_register(uint8_t vector, irq_handler_t handler) {
    if (vector < IRQ_VECTOR_BASE) {
        return;
    }
    handlers[vector - IRQ_VECTOR_BASE] = handler;
}

void irq_dispatch(struct trapframe *tf) {
    uint8_t vector = (uint8_t)tf->vector;

    if (vector == IRQ_VECTOR_SPURIOUS) {
        return;
    }

    irq_handler_t handler = handlers[vector - IRQ_VECTOR_BASE];
    if (handler) {
        handler(tf);
    } else {
        printf("irq: unhandled vector 0x%lx\n", (unsigned long)vector);
    }

    apic_eoi();
}
