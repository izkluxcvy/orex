#include <string.h>

char *stpncpy(char *__restrict dst, const char *__restrict src, size_t n) {
    size_t len = strnlen(src, n);
    memcpy(dst, src, len);
    memset(dst + len, 0, n - len);
    return dst + len;
}
