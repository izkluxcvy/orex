#pragma once

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("out dx, al" : : "d"(port), "a"(val));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ __volatile__("in al, dx" : "=a"(val) : "d"(port));
    return val;
}
