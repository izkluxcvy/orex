#include <string.h>

char *strcat(char *__restrict dst, const char *__restrict src) {
    strcpy(dst + strlen(dst), src);
    return dst;
}
