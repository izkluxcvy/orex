#include <stdint.h>
#include <string.h>

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    if ((uintptr_t)d - (uintptr_t)s >= n) {
        return memcpy(dst, src, n);
    }
    while (n--) {
        d[n] = s[n];
    }
    return dst;
}
