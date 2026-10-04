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

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t       *d = dest;
    const uint8_t *s = src;
    if (d <= s || d >= s + n) {
        return memcpy(dest, src, n);
    }

    uint8_t       *de = d + n - 1;
    const uint8_t *se = s + n - 1;
    size_t         q = n / 8, r = n % 8;
    __asm__ __volatile__("std\n\t"
                         "rep movsb\n\t"
                         "sub rdi, 7\n\t"
                         "sub rsi, 7\n\t"
                         "mov rcx, %3\n\t"
                         "rep movsq\n\t"
                         "cld"
                         : "+D"(de), "+S"(se), "+c"(r)
                         : "r"(q)
                         : "memory", "cc");
    return dest;
}
