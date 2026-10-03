#pragma once

#include <stdint.h>

struct cpuid {
    uint32_t a, b, c, d;
};

static inline struct cpuid cpuid(uint32_t leaf) {
    struct cpuid r;
    __asm__ __volatile__("cpuid"
                         : "=a"(r.a), "=b"(r.b), "=c"(r.c), "=d"(r.d)
                         : "a"(leaf));
    return r;
}

static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
