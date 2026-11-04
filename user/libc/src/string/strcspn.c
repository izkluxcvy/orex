#include <string.h>

size_t strcspn(const char *s, const char *reject) {
    size_t n = 0;
    while (s[n] && !strchr(reject, s[n])) {
        n++;
    }
    return n;
}
