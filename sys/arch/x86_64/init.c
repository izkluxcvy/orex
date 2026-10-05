#include <cpu.h>
#include <ktime.h>
#include <machdep.h>
#include <printf.h>
#include <sched.h>

#include "apic.h"
#include "cpufunc.h"
#include "irq.h"
#include "segment.h"
#include "serial.h"
#include "trap.h"

static volatile uint64_t ticks;

static void timer_handler(struct trapframe *tf) {
    if (curcpu()->id == 0) {
        ticks++;
    }
    time_tick(tf->cs & 3);
    sched_tick();
}

uint64_t machdep_ticks() { return ticks; }
uint64_t machdep_cycles() { return rdtsc(); }
uint64_t machdep_cycles_hz() { return tsc_hz; }

void machdep_init() {
    serial_init();
    gdt_init();
    idt_init();
    printf("machdep: initialized\n");
}

void machdep_init_late() {
    irq_init();
    apic_init();
    irq_register(IRQ_VECTOR_TIMER, timer_handler);
    apic_timer_init(TICK_HZ);

    __asm__ __volatile__("sti");
    printf("machdep: interrupts enabled\n");
}
