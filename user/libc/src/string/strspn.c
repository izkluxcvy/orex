#include <string.h>

size_t strspn(const char *s, const char *accept) {
    size_t n = 0;
    while (s[n] && strchr(accept, s[n])) {
        n++;
    }
    return n;
}
