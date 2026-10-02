#pragma once

#include <stdint.h>

// byte
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("out %1, %0" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ __volatile__("in %0, %1" : "=a"(val) : "Nd"(port));
    return val;
}

// word
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__("out %1, %0" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ __volatile__("in %0, %1" : "=a"(val) : "Nd"(port));
    return val;
}

// long
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ __volatile__("out %1, %0" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ __volatile__("in %0, %1" : "=a"(val) : "Nd"(port));
    return val;
}
