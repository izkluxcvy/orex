#include <ctype.h>
#include <strings.h>

int strcasecmp(const char *a, const char *b) {
    const unsigned char *p = (const unsigned char *)a;
    const unsigned char *q = (const unsigned char *)b;
    while (*p && tolower(*p) == tolower(*q)) {
        p++, q++;
    }
    return tolower(*p) - tolower(*q);
}

int strcasecmp_l(const char *a, const char *b, locale_t loc) {
    (void)loc;
    return strcasecmp(a, b);
}
