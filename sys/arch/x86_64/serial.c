#include <stdint.h>

#include <printf.h>
#include <tty.h>

#include "io.h"
#include "ioapic.h"
#include "irq.h"
#include "serial.h"

#define COM1_PORT 0x3f8
#define COM1_IRQ  4

static struct tty ttys0;

static int transmit_ready() { return inb(COM1_PORT + 5) & 0x20; }

void serial_putc(char c) {
    while (!transmit_ready()) {
    }
    outb(COM1_PORT, (uint8_t)c);
}

static size_t serial_output(struct tty *t, const char *p, size_t n) {
    (void)t;
    for (size_t i = 0; i < n; i++) {
        serial_putc(p[i]);
    }
    return n;
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
        tty_input(&ttys0, (char)inb(COM1_PORT));
    }
}

void serial_init_late() {
    tty_init(&ttys0, serial_output);
    irq_register(IRQ_VECTOR_ISA + COM1_IRQ, serial_irq);
    ioapic_route_isa(COM1_IRQ, IRQ_VECTOR_ISA + COM1_IRQ);
    outb(COM1_PORT + 1, 0x01);
}

struct tty *tty_serial() { return ttys0.output ? &ttys0 : nullptr; }
