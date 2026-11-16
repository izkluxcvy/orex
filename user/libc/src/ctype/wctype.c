#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include <locale_impl.h>

struct __wc_case {
    unsigned start;
    unsigned count, delta, step;
};

#include "unicode_tab.h"

#define W_ALPHA 1
#define W_UPPER 2
#define W_LOWER 4
#define W_SPACE 8
#define W_BLANK 16
#define W_CNTRL 32
#define W_PRINT 64
#define W_PUNCT 128

static uint32_t lookup(wint_t c) {
    unsigned lo = 0;
    unsigned hi = __wc_class_n;
    while (hi - lo > 1) {
        unsigned mid = (lo + hi) / 2;
        if ((__wc_class[mid] & 0x1fffff) <= c) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return __wc_class[lo];
}

static unsigned props(wint_t c, locale_t l) {
    if (c >= (UTF8_L(l) ? 0x110000U : 0x80U)) {
        return 0;
    }
    return (lookup(c) >> 21) & 0xff;
}

enum {
    T_ALNUM = 1,
    T_ALPHA,
    T_BLANK,
    T_CNTRL,
    T_DIGIT,
    T_GRAPH,
    T_LOWER,
    T_PRINT,
    T_PUNCT,
    T_SPACE,
    T_UPPER,
    T_XDIGIT
};

static const char *const type_names[] = {
    "",      "alnum", "alpha", "blank", "cntrl", "digit",  "graph",
    "lower", "print", "punct", "space", "upper", "xdigit",
};

int iswctype_l(wint_t c, wctype_t t, locale_t l) {
    unsigned p = props(c, l);
    switch (t) {
    case T_ALNUM:
        return (p & W_ALPHA) || (c >= '0' && c <= '9');
    case T_ALPHA:
        return !!(p & W_ALPHA);
    case T_BLANK:
        return !!(p & W_BLANK);
    case T_CNTRL:
        return !!(p & W_CNTRL);
    case T_DIGIT:
        return c >= '0' && c <= '9';
    case T_GRAPH:
        return (p & W_PRINT) && !(p & W_SPACE);
    case T_LOWER:
        return !!(p & W_LOWER);
    case T_PRINT:
        return !!(p & W_PRINT);
    case T_PUNCT:
        return !!(p & W_PUNCT);
    case T_SPACE:
        return !!(p & W_SPACE);
    case T_UPPER:
        return !!(p & W_UPPER);
    case T_XDIGIT:
        return (c >= '0' && c <= '9') ||
               ((c | 0x20) >= 'a' && (c | 0x20) <= 'f');
    }
    return 0;
}

wctype_t wctype_l(const char *name, locale_t l) {
    (void)l;
    for (wctype_t t = T_ALNUM; t <= T_XDIGIT; t++) {
        if (!strcmp(name, type_names[t])) {
            return t;
        }
    }
    return 0;
}

static wint_t map(wint_t c, const struct __wc_case *tab, unsigned n) {
    for (unsigned i = 0; i < n && tab[i].start <= c; i++) {
        unsigned off = c - tab[i].start;
        if (off % (unsigned)tab[i].step == 0 &&
            off / (unsigned)tab[i].step < (unsigned)tab[i].count) {
            return c + (wint_t)tab[i].delta;
        }
    }
    return c;
}

wint_t towupper_l(wint_t c, locale_t l) {
    if (c < 0x80) {
        return c >= 'a' && c <= 'z' ? c - 32 : c;
    }
    return UTF8_L(l) && c < 0x110000 ? map(c, __wc_toupper, __wc_toupper_n) : c;
}

wint_t towlower_l(wint_t c, locale_t l) {
    if (c < 0x80) {
        return c >= 'A' && c <= 'Z' ? c + 32 : c;
    }
    return UTF8_L(l) && c < 0x110000 ? map(c, __wc_tolower, __wc_tolower_n) : c;
}

wctrans_t wctrans_l(const char *name, locale_t l) {
    (void)l;
    return !strcmp(name, "toupper") ? 1 : !strcmp(name, "tolower") ? 2 : 0;
}

wint_t towctrans_l(wint_t c, wctrans_t t, locale_t l) {
    return t == 1 ? towupper_l(c, l) : t == 2 ? towlower_l(c, l) : c;
}

int __wcwidth_l(wchar_t wc, locale_t l) {
    wint_t c = (wint_t)wc;
    if (!c) {
        return 0;
    }
    if (c >= (UTF8_L(l) ? 0x110000U : 0x80U)) {
        return -1;
    }
    int w = (int)(lookup(c) >> 29) & 3;
    return w == 3 ? -1 : w;
}

int wcwidth(wchar_t c) { return __wcwidth_l(c, __cur_locale()); }

int wcswidth(const wchar_t *s, size_t n) {
    int total = 0;
    for (; n-- && *s; s++) {
        int w = wcwidth(*s);
        if (w < 0) {
            return -1;
        }
        total += w;
    }
    return total;
}

#define CLASS(name, t)                                                         \
    int isw##name##_l(wint_t c, locale_t l) { return iswctype_l(c, t, l); }    \
    int isw##name(wint_t c) { return iswctype_l(c, t, __cur_locale()); }

CLASS(alnum, T_ALNUM)
CLASS(alpha, T_ALPHA)
CLASS(blank, T_BLANK)
CLASS(cntrl, T_CNTRL)
CLASS(digit, T_DIGIT)
CLASS(graph, T_GRAPH)
CLASS(lower, T_LOWER)
CLASS(print, T_PRINT)
CLASS(punct, T_PUNCT)
CLASS(space, T_SPACE)
CLASS(upper, T_UPPER)
CLASS(xdigit, T_XDIGIT)

int iswctype(wint_t c, wctype_t t) { return iswctype_l(c, t, __cur_locale()); }
wctype_t wctype(const char *name) { return wctype_l(name, NULL); }
wint_t towupper(wint_t c) { return towupper_l(c, __cur_locale()); }
wint_t towlower(wint_t c) { return towlower_l(c, __cur_locale()); }
wctrans_t wctrans(const char *name) { return wctrans_l(name, NULL); }
wint_t towctrans(wint_t c, wctrans_t t) {
    return towctrans_l(c, t, __cur_locale());
}
