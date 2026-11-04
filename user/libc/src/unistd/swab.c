#include <unistd.h>

void swab(const void *__restrict src, void *__restrict dst, ssize_t n) {
    const unsigned char *s = src;
    unsigned char *d = dst;
    for (; n > 1; n -= 2, s += 2, d += 2) {
        d[0] = s[1];
        d[1] = s[0];
    }
}
