#include <string.h>

char *strncpy(char *__restrict dst, const char *__restrict src, size_t n) {
    stpncpy(dst, src, n);
    return dst;
}
