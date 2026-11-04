#include <string.h>

void *memcpy(void *__restrict dst, const void *__restrict src, size_t n) {
    void *d = dst;
    size_t q = n / 8, r = n % 8;
    __asm__ __volatile__("rep movsq"
                         : "+D"(d), "+S"(src), "+c"(q)
                         :
                         : "memory");
    __asm__ __volatile__("rep movsb"
                         : "+D"(d), "+S"(src), "+c"(r)
                         :
                         : "memory");
    return dst;
}
