#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dest, const void *src, size_t n) {
    void       *d = dest;
    const void *s = src;
    size_t      q = n / 8, r = n % 8;
    __asm__ __volatile__("rep movsq" : "+D"(d), "+S"(s), "+c"(q) : : "memory");
    __asm__ __volatile__("rep movsb" : "+D"(d), "+S"(s), "+c"(r) : : "memory");
    return dest;
}

void memset(void *dest, int value, size_t n) {
    uint64_t v = (uint8_t)value * 0x0101010101010101ULL;
    size_t   q = n / 8, r = n % 8;
    __asm__ __volatile__("rep stosq" : "+D"(dest), "+c"(q) : "a"(v) : "memory");
    __asm__ __volatile__("rep stosb" : "+D"(dest), "+c"(r) : "a"(v) : "memory");
}
