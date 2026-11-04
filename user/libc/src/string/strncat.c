#include <string.h>

char *strncat(char *__restrict dst, const char *__restrict src, size_t n) {
    char *d = dst + strlen(dst);
    size_t len = strnlen(src, n);
    memcpy(d, src, len);
    d[len] = '\0';
    return dst;
}
