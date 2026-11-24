#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char digits[] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

long a64l(const char *s) {
    uint32_t v = 0;
    for (int i = 0; i < 6 && s[i]; i++) {
        const char *d = strchr(digits, s[i]);
        if (!d || !*d) {
            break;
        }
        v |= (uint32_t)(d - digits) << (6 * i);
    }
    return (int32_t)v;
}

char *l64a(long value) {
    static char buf[7];
    uint32_t v = (uint32_t)value;
    int i = 0;
    for (; v; v >>= 6) {
        buf[i++] = digits[v % 63];
    }
    buf[i] = '\0';
    return buf;
}
