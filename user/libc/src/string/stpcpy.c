#include <string.h>

char *stpcpy(char *__restrict dst, const char *__restrict src) {
    while ((*dst = *src++)) {
        dst++;
    }
    return dst;
}
