#include <stdint.h>

#include "io.h"
#include "serial.h"
#include <printf.h>

#define COM1_PORT 0x3f8

static int transmit_ready(void) { return inb(COM1_PORT + 5) & 0x20; }

void serial_putc(char c) {
    while (!transmit_ready()) {
    }
    outb(COM1_PORT, (uint8_t)c);
}

void serial_init(void) {
    outb(COM1_PORT + 1, 0x00); // Disable interrupts
    outb(COM1_PORT + 3, 0x80); // Enable DLAB
    outb(COM1_PORT + 0, 0x03); // Divisor low byte: 38400 baud
    outb(COM1_PORT + 1, 0x00); // Divisor high byte
    outb(COM1_PORT + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(COM1_PORT + 2, 0xc7); // Enable FIFO, clear, 14-byte threshold
    outb(COM1_PORT + 4, 0x0b); // IRQs enabled, RTS/DSR set

    printf_putc = serial_putc;
}
