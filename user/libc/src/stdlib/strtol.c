#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>

static unsigned long long parse(const char *s, char **end, int base,
                                unsigned long long lim, int is_signed) {
    const char *p = s;
    if (base < 0 || base == 1 || base > 36) {
        errno = EINVAL;
        if (end) {
            *end = (char *)s;
        }
        return 0;
    }

    while (isspace((unsigned char)*p)) {
        p++;
    }
    int neg = *p == '-';
    if (*p == '-' || *p == '+') {
        p++;
    }
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] | 0x20) == 'x' &&
        isxdigit((unsigned char)p[2])) {
        p += 2;
        base = 16;
    } else if (base == 0) {
        base = *p == '0' ? 8 : 10;
    }

    unsigned long long v = 0;
    unsigned long long max = is_signed ? lim + neg : lim;
    int any = 0, over = 0;
    for (;; p++) {
        int d = isdigit((unsigned char)*p)   ? *p - '0'
                : isalpha((unsigned char)*p) ? (*p | 0x20) - 'a' + 10
                                             : 99;
        if (d >= base) {
            break;
        }
        any = 1;
        if (v > (max - (unsigned)d) / (unsigned)base) {
            over = 1;
        } else {
            v = v * (unsigned)base + (unsigned)d;
        }
    }
    if (end) {
        *end = (char *)(any ? p : s);
    }
    if (over) {
        errno = ERANGE;
        return is_signed ? (neg ? -max : max) : lim;
    }
    return neg ? -v : v;
}

long strtol(const char *__restrict s, char **__restrict end, int base) {
    return (long)parse(s, end, base, LONG_MAX, 1);
}

long long strtoll(const char *__restrict s, char **__restrict end, int base) {
    return (long long)parse(s, end, base, LLONG_MAX, 1);
}

unsigned long strtoul(const char *__restrict s, char **__restrict end,
                      int base) {
    return (unsigned long)parse(s, end, base, ULONG_MAX, 0);
}

unsigned long long strtoull(const char *__restrict s, char **__restrict end,
                            int base) {
    return parse(s, end, base, ULLONG_MAX, 0);
}

intmax_t strtoimax(const char *__restrict s, char **__restrict end, int base) {
    return (intmax_t)parse(s, end, base, INTMAX_MAX, 1);
}

uintmax_t strtoumax(const char *__restrict s, char **__restrict end, int base) {
    return (uintmax_t)parse(s, end, base, UINTMAX_MAX, 0);
}

int atoi(const char *s) { return (int)strtol(s, nullptr, 10); }
long atol(const char *s) { return strtol(s, nullptr, 10); }
long long atoll(const char *s) { return strtoll(s, nullptr, 10); }
