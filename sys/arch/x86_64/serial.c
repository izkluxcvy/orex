#include <stdint.h>

#include <printf.h>

#include "io.h"
#include "ioapic.h"
#include "irq.h"
#include "serial.h"

#define COM1_PORT 0x3f8
#define COM1_IRQ  4

static int transmit_ready() { return inb(COM1_PORT + 5) & 0x20; }

void serial_putc(char c) {
    while (!transmit_ready()) {
    }
    outb(COM1_PORT, (uint8_t)c);
}

void serial_init() {
    outb(COM1_PORT + 1, 0x00); // Disable all interrupts
    outb(COM1_PORT + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outb(COM1_PORT + 0, 0x01); // Set divisor to 1 (lo byte) 115200 baud
    outb(COM1_PORT + 1, 0x00); //                  (hi byte)
    outb(COM1_PORT + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(COM1_PORT + 2, 0xc7); // Enable FIFO, clear, 14-byte threshold
    outb(COM1_PORT + 4, 0x0b); // IRQs enabled, RTS/DSR set

    printf_add_sink(serial_putc);
}

static void serial_irq(struct trapframe *tf) {
    (void)tf;
    while (inb(COM1_PORT + 5) & 0x01) {
        printf("serial: %x\n", (unsigned)inb(COM1_PORT));
    }
}

void serial_init_late() {
    irq_register(IRQ_VECTOR_ISA + COM1_IRQ, serial_irq);
    ioapic_route_isa(COM1_IRQ, IRQ_VECTOR_ISA + COM1_IRQ);
    outb(COM1_PORT + 1, 0x01);
}
