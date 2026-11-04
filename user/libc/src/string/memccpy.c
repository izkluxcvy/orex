#include <string.h>

void *memccpy(void *__restrict dst, const void *__restrict src, int c,
              size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (; n; n--) {
        if ((*d++ = *s++) == (unsigned char)c) {
            return d;
        }
    }
    return NULL;
}
