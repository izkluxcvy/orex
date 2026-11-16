#include <ctype.h>
#include <strings.h>

int strncasecmp(const char *a, const char *b, size_t n) {
    const unsigned char *p = (const unsigned char *)a;
    const unsigned char *q = (const unsigned char *)b;
    for (; n; n--, p++, q++) {
        if (tolower(*p) != tolower(*q)) {
            return tolower(*p) - tolower(*q);
        }
    }
    return 0;
}

int strncasecmp_l(const char *a, const char *b, size_t n, locale_t loc) {
    (void)loc;
    return strncasecmp(a, b, n);
}
