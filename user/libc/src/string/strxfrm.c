#include <string.h>

size_t strxfrm(char *__restrict dst, const char *__restrict src, size_t n) {
    size_t len = strlen(src);
    if (len < n) {
        memcpy(dst, src, len + 1);
    }
    return len;
}

size_t strxfrm_l(char *__restrict dst, const char *__restrict src, size_t n,
                 locale_t loc) {
    (void)loc;
    return strxfrm(dst, src, n);
}
