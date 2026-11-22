#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static char *narrow(const wchar_t *s, size_t *skip, char *small, size_t cap) {
    size_t i = 0, n = 0;
    while (iswspace((wint_t)s[i])) {
        i++;
    }
    while (s[i + n] > 0 && s[i + n] < 0x80) {
        n++;
    }
    char *b = n < cap ? small : malloc(n + 1);
    if (!b) {
        return nullptr;
    }
    for (size_t k = 0; k < n; k++) {
        b[k] = (char)s[i + k];
    }
    b[n] = 0;
    *skip = i;
    return b;
}

static void done(const wchar_t *s, wchar_t **end, size_t skip, char *b,
                 const char *e, char *small) {
    if (end) {
        *end = (wchar_t *)(e == b ? s : s + skip + (e - b));
    }
    if (b != small) {
        free((char *)b);
    }
}

#define FLOAT(name, T, narrow_fn)                                              \
    T name(const wchar_t *__restrict s, wchar_t **__restrict end) {            \
        char small[128], *e;                                                   \
        size_t skip;                                                           \
        char *b = narrow(s, &skip, small, sizeof(small));                      \
        if (!b) {                                                              \
            return 0;                                                          \
        }                                                                      \
        T v = narrow_fn(b, &e);                                                \
        done(s, end, skip, b, e, small);                                       \
        return v;                                                              \
    }

FLOAT(wcstof, float, strtof)
FLOAT(wcstod, double, strtod)
FLOAT(wcstold, long double, strtold)

#define INT(name, T, narrow_fn)                                                \
    T name(const wchar_t *__restrict s, wchar_t **__restrict end, int base) {  \
        char small[128], *e;                                                   \
        size_t skip;                                                           \
        char *b = narrow(s, &skip, small, sizeof small);                       \
        if (!b) {                                                              \
            return 0;                                                          \
        }                                                                      \
        T v = narrow_fn(b, &e, base);                                          \
        done(s, end, skip, b, e, small);                                       \
        return v;                                                              \
    }

INT(wcstol, long, strtol)
INT(wcstoll, long long, strtoll)
INT(wcstoul, unsigned long, strtoul)
INT(wcstoull, unsigned long long, strtoull)
INT(wcstoimax, intmax_t, strtoimax)
INT(wcstoumax, uintmax_t, strtoumax)
