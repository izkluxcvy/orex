#include <string.h>

size_t strlcat(char *__restrict dst, const char *__restrict src, size_t n) {
    size_t have = strnlen(dst, n);
    if (have == n) {
        return n + strlen(src);
    }
    return have + strlcpy(dst + have, src, n - have);
}
