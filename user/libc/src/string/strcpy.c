#include <string.h>

char *strcpy(char *__restrict dst, const char *__restrict src) {
    stpcpy(dst, src);
    return dst;
}
