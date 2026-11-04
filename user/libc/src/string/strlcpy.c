#include <string.h>

size_t strlcpy(char *__restrict dst, const char *__restrict src, size_t n) {
    size_t len = strlen(src);
    if (n) {
        size_t k = len < n - 1 ? len : n - 1;
        memcpy(dst, src, k);
        dst[k] = '\0';
    }
    return len;
}
