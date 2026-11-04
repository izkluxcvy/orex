#include <stdint.h>
#include <string.h>

void *memset(void *dst, int c, size_t n) {
    void *d = dst;
    uint64_t v = (unsigned char)c * 0x0101010101010101ull;
    size_t q = n / 8, r = n % 8;
    __asm__ __volatile__("rep stosq" : "+D"(d), "+c"(q) : "a"(v) : "memory");
    __asm__ __volatile__("rep stosb" : "+D"(d), "+c"(r) : "a"(v) : "memory");
    return dst;
}

void *memset_explicit(void *dst, int c, size_t n) {
    memset(dst, c, n);
    __asm__ __volatile__("" : : "r"(dst) : "memory");
    return dst;
}
