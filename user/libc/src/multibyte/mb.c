#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>
#include <wchar.h>

#include <locale_impl.h>

#define LEFT(st)  ((st)->__left & 7)
#define TOTAL(st) (((st)->__left >> 4) & 7)

static size_t ilseq(mbstate_t *st) {
    st->__left = st->__bits = 0;
    errno = EILSEQ;
    return (size_t)-1;
}

static int second_ok(unsigned total, unsigned bits, unsigned char c) {
    if (total == 3) {
        return bits == 0 ? c >= 0xa0 : bits == 0xd ? c < 0xa0 : 1;
    }
    if (total == 4) {
        return bits == 0 ? c >= 0x90 : bits == 4 ? c < 0x90 : 1;
    }
    return 1;
}

size_t mbrtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n,
               mbstate_t *__restrict st) {
    static mbstate_t internal;
    if (!st) {
        st = &internal;
    }
    if (!s) {
        pwc = nullptr;
        s = "";
        n = 1;
    }
    if (!n) {
        return (size_t)-2;
    }
    if (!UTF8) {
        wchar_t c = (unsigned char)*s;
        if (pwc) {
            *pwc = c;
        }
        return c != 0;
    }

    size_t i = 0;
    unsigned bits = st->__bits;
    unsigned left = LEFT(st);
    unsigned total = TOTAL(st);
    if (!left) {
        unsigned char c = (unsigned char)s[i++];
        // ascii
        if (c < 0x80) {
            if (pwc) {
                *pwc = c;
            }
            return c != 0;
        }

        if (c < 0xc2 || c > 0xf4) {
            return ilseq(st);
        }
        total = c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
        left = total - 1;
        bits = c & (0x7f >> total);
    }
    for (; left && i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c & 0xc0) != 0x80 ||
            (left == total - 1 && !second_ok(total, bits, c))) {
            return ilseq(st);
        }
        bits = bits << 6 | (c & 0x3f);
        left--;
    }
    if (left) {
        st->__bits = bits;
        st->__left = left | total << 4;
        return (size_t)-2;
    }
    st->__bits = st->__left = 0;
    if (pwc) {
        *pwc = (wchar_t)bits;
    }
    return bits ? i : 0;
}

size_t mbrlen(const char *__restrict s, size_t n, mbstate_t *__restrict st) {
    static mbstate_t internal;
    return mbrtowc(nullptr, s, n, st ? st : &internal);
}

int mbsinit(const mbstate_t *st) { return !st || !st->__left; }

size_t wcrtomb(char *__restrict s, wchar_t wc, mbstate_t *__restrict st) {
    (void)st;
    char buf[4];
    if (!s) {
        s = buf;
        wc = 0;
    }
    unsigned c = (unsigned)wc;
    if (!UTF8) {
        if (c > 0xff) {
            errno = EILSEQ;
            return (size_t)-1;
        }
        *s = (char)c;
        return 1;
    }

    if (c < 0x80) {
        *s = (char)c;
        return 1;
    }
    if (c < 0x800) {
        s[0] = (char)(0xc0 | c >> 6);
        s[1] = (char)(0x80 | (c & 0x3f));
        return 2;
    }
    if ((c >= 0xd800 && c < 0xe000) || c > 0x10ffff) {
        errno = EILSEQ;
        return (size_t)-1;
    }
    if (c < 0x10000) {
        s[0] = (char)(0xe0 | c >> 12);
        s[1] = (char)(0x80 | ((c >> 6) & 0x3f));
        s[2] = (char)(0x80 | (c & 0x3f));
        return 3;
    }
    s[0] = (char)(0xf0 | c >> 18);
    s[1] = (char)(0x80 | ((c >> 12) & 0x3f));
    s[2] = (char)(0x80 | ((c >> 6) & 0x3f));
    s[3] = (char)(0x80 | (c & 0x3f));
    return 4;
}

wint_t btowc(int c) {
    if (c == EOF) {
        return WEOF;
    }
    c = (unsigned char)c;
    return !UTF8 || c < 0x80 ? (wint_t)c : WEOF;
}

int wctob(wint_t wc) {
    if (wc < (UTF8 ? 0x80U : 0x100U)) {
        return (int)wc;
    }
    return EOF;
}

size_t mbsnrtowcs(wchar_t *__restrict dst, const char **__restrict src,
                  size_t nms, size_t len, mbstate_t *__restrict st) {
    static mbstate_t internal;
    if (!st) {
        st = &internal;
    }
    const char *s = *src;
    size_t n = 0;
    while (!dst || n < len) {
        wchar_t wc;
        size_t k = mbrtowc(&wc, s, nms, st);
        if (k == (size_t)-1) {
            if (dst) {
                *src = s;
            }
            return k;
        }
        if (k == (size_t)-2) {
            s += nms;
            break;
        }
        if (dst) {
            dst[n] = wc;
        }
        if (!wc) {
            s = nullptr;
            break;
        }
        s += k;
        nms -= k;
        n++;
    }
    if (dst) {
        *src = s;
    }
    return n;
}

size_t mbsrtowcs(wchar_t *__restrict dst, const char **__restrict src,
                 size_t len, mbstate_t *__restrict st) {
    return mbsnrtowcs(dst, src, SIZE_MAX, len, st);
}

size_t wcsnrtombs(char *__restrict dst, const wchar_t **__restrict src,
                  size_t nwc, size_t len, mbstate_t *__restrict st) {
    const wchar_t *s = *src;
    size_t n = 0;
    for (; nwc; nwc--, s++) {
        char buf[4];
        size_t k = wcrtomb(buf, *s, st);
        if (k == (size_t)-1) {
            if (dst) {
                *src = s;
            }
            return k;
        }
        if (dst) {
            if (n + k > len) {
                break;
            }
            for (size_t i = 0; i < k; i++) {
                dst[n + i] = buf[i];
            }
        }
        if (!*s) {
            s = nullptr;
            break;
        }
        n += k;
    }
    if (dst) {
        *src = s;
    }
    return n;
}

size_t wcsrtombs(char *__restrict dst, const wchar_t **__restrict src,
                 size_t len, mbstate_t *__restrict st) {
    return wcsnrtombs(dst, src, SIZE_MAX, len, st);
}

/*
 * stateless functions of stdlib.h
 */

int mbtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n) {
    if (!s) {
        return 0;
    }
    mbstate_t st = {0};
    size_t k = mbrtowc(pwc, s, n, &st);
    if (k == (size_t)-2) {
        errno = EILSEQ;
        return -1;
    }
    return (int)k;
}

int mblen(const char *s, size_t n) { return mbtowc(nullptr, s, n); }

int wctomb(char *s, wchar_t wc) {
    if (!s) {
        return 0;
    }
    return (int)wcrtomb(s, wc, nullptr);
}

size_t mbstowcs(wchar_t *__restrict dst, const char *__restrict src, size_t n) {
    mbstate_t st = {0};
    const char *s = src;
    return mbsrtowcs(dst, &s, n, &st);
}

size_t wcstombs(char *__restrict dst, const wchar_t *__restrict src, size_t n) {
    mbstate_t st = {0};
    const wchar_t *s = src;
    return wcsrtombs(dst, &s, n, &st);
}

/*
 * uchar.h
 */

size_t mbrtoc32(char32_t *__restrict pc, const char *__restrict s, size_t n,
                mbstate_t *__restrict st) {
    static mbstate_t internal;
    wchar_t wc;
    size_t k = mbrtowc(&wc, s, n, st ? st : &internal);
    if (pc && s && k <= 4) {
        *pc = (char32_t)wc;
    }
    return k;
}

size_t c32rtomb(char *__restrict s, char32_t c, mbstate_t *__restrict st) {
    return wcrtomb(s, (wchar_t)c, st);
}

#define LOW_PENDING 0x100

size_t mbrtoc16(char16_t *__restrict pc, const char *__restrict s, size_t n,
                mbstate_t *__restrict st) {
    static mbstate_t internal;
    if (!st) {
        st = &internal;
    }
    if (st->__left == LOW_PENDING) {
        if (pc) {
            *pc = (char16_t)st->__bits;
        }
        st->__left = st->__bits = 0;
        return (size_t)-3;
    }

    wchar_t wc;
    size_t k = mbrtowc(&wc, s, n, st);
    if (k > 4 || !s) {
        return k;
    }
    if ((unsigned)wc >= 0x10000) {
        unsigned v = (unsigned)wc - 0x10000;
        if (pc) {
            *pc = (char16_t)(0xd800 + (v >> 10));
        }
        st->__bits = 0xdc00 + (v & 0x3ff);
        st->__left = LOW_PENDING;
    } else if (pc) {
        *pc = (char16_t)wc;
    }
    return k;
}

size_t c16rtomb(char *__restrict s, char16_t c, mbstate_t *__restrict st) {
    static mbstate_t internal;
    if (!st) {
        st = &internal;
    }
    if (!s) {
        st->__bits = 0;
        return 1;
    }
    if (st->__bits) {
        unsigned hi = st->__bits;
        st->__bits = 0;
        if (c < 0xdc00 || c >= 0xe000) {
            errno = EILSEQ;
            return (size_t)-1;
        }
        return wcrtomb(
            s, (wchar_t)(0x10000 + ((hi - 0xd800) << 10) + (c - 0xdc00)),
            nullptr);
    }
    if (c >= 0xd800 && c < 0xdc00) {
        st->__bits = c;
        return 0;
    }
    return wcrtomb(s, (wchar_t)c, nullptr);
}
