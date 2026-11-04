#include <string.h>

void *memchr(const void *s, int c, size_t n) {
    for (const unsigned char *p = s; n; n--, p++) {
        if (*p == (unsigned char)c) {
            return (void *)p;
        }
    }
    return nullptr;
}
