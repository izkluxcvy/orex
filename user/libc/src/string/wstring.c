#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include <locale_impl.h>

size_t wcslen(const wchar_t *s) {
    const wchar_t *p = s;
    while (*p) {
        p++;
    }
    return (size_t)(p - s);
}

size_t wcsnlen(const wchar_t *s, size_t n) {
    size_t i = 0;
    while (i < n && s[i]) {
        i++;
    }
    return i;
}

wchar_t *wcscpy(wchar_t *__restrict d, const wchar_t *__restrict s) {
    wchar_t *r = d;
    while ((*d++ = *s++)) {
    }
    return r;
}

wchar_t *wcpcpy(wchar_t *__restrict d, const wchar_t *__restrict s) {
    while ((*d++ = *s++)) {
        d++;
    }
    return d;
}

wchar_t *wcpncpy(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n) {
    size_t k = wcsnlen(s, n);
    wmemcpy(d, s, k);
    wmemset(d + k, 0, n - k);
    return d + k;
}

wchar_t *wcsncpy(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n) {
    wcpncpy(d, s, n);
    return d;
}

wchar_t *wcscat(wchar_t *__restrict d, const wchar_t *__restrict s) {
    wcscpy(d + wcslen(d), s);
    return d;
}

wchar_t *wcsncat(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n) {
    wchar_t *e = d + wcslen(d);
    size_t k = wcsnlen(s, n);
    wmemcpy(e, s, k);
    e[k] = 0;
    return d;
}

size_t wcslcpy(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n) {
    size_t len = wcslen(s);
    if (n) {
        size_t k = len < n - 1 ? len : n - 1;
        wmemcpy(d, s, k);
        d[k] = 0;
    }
    return len;
}

size_t wcslcat(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n) {
    size_t have = wcsnlen(d, n);
    if (have == n) {
        return n + wcslen(s);
    }
    return have + wcslcpy(d + have, s, n - have);
}

int wcscmp(const wchar_t *a, const wchar_t *b) {
    for (; *a == *b && *a; a++, b++) {
    }
    return *a < *b ? -1 : *a > *b;
}

int wcsncmp(const wchar_t *a, const wchar_t *b, size_t n) {
    for (; n && *a == *b && *a; a++, b++, n--) {
    }
    return !n ? 0 : *a < *b ? -1 : *a > *b;
}

int wcsncasecmp_l(const wchar_t *a, const wchar_t *b, size_t n, locale_t l) {
    for (; n; a++, b++, n--) {
        wint_t x = towlower_l((wint_t)*a, l);
        wint_t y = towlower_l((wint_t)*b, l);
        if (x != y || !x) {
            return x < y ? -1 : x > y;
        }
    }
    return 0;
}

int wcscasecmp_l(const wchar_t *a, const wchar_t *b, locale_t l) {
    return wcsncasecmp_l(a, b, SIZE_MAX, l);
}

int wcscasecmp(const wchar_t *a, const wchar_t *b) {
    return wcscasecmp_l(a, b, __cur_locale());
}

int wcsncasecmp(const wchar_t *a, const wchar_t *b, size_t n) {
    return wcsncasecmp_l(a, b, n, __cur_locale());
}

int wcscoll_l(const wchar_t *a, const wchar_t *b, locale_t l) {
    (void)l;
    return wcscmp(a, b);
}

int wcscoll(const wchar_t *a, const wchar_t *b) { return wcscmp(a, b); }

size_t wcsxfrm_l(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n,
                 locale_t l) {
    (void)l;
    size_t len = wcslen(s);
    if (len < n) {
        wmemcpy(d, s, len + 1);
    }
    return len;
}

size_t wcsxfrm(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n) {
    return wcsxfrm_l(d, s, n, __cur_locale());
}

wchar_t *wcschr(const wchar_t *s, wchar_t c) {
    for (;; s++) {
        if (*s == c) {
            return (wchar_t *)s;
        }
        if (!*s) {
            return nullptr;
        }
    }
}

wchar_t *wcsrchr(const wchar_t *s, wchar_t c) {
    const wchar_t *r = nullptr;
    for (;; s++) {
        if (*s == c) {
            r = s;
        }
        if (!*s) {
            return (wchar_t *)r;
        }
    }
}

size_t wcsspn(const wchar_t *s, const wchar_t *set) {
    size_t i = 0;
    while (s[i] && wcschr(set, s[i])) {
        i++;
    }
    return i;
}

size_t wcscspn(const wchar_t *s, const wchar_t *set) {
    size_t i = 0;
    while (s[i] && !wcschr(set, s[i])) {
        i++;
    }
    return i;
}

wchar_t *wcspbrk(const wchar_t *s, const wchar_t *set) {
    s += wcscspn(s, set);
    return *s ? (wchar_t *)s : nullptr;
}

wchar_t *wcsstr(const wchar_t *__restrict h, const wchar_t *__restrict n) {
    size_t k = wcslen(n);
    if (!k) {
        return (wchar_t *)h;
    }
    for (; (h = wcschr(h, *n)); h++) {
        if (!wcsncmp(h, n, k)) {
            return (wchar_t *)h;
        }
    }
    return nullptr;
}

wchar_t *wcstok(wchar_t *__restrict s, const wchar_t *__restrict sep,
                wchar_t **__restrict save) {
    if (!s && !(s = *save)) {
        return nullptr;
    }
    s += wcsspn(s, sep);
    if (!*s) {
        *save = nullptr;
        return nullptr;
    }
    wchar_t *e = s + wcscspn(s, sep);
    if (*e) {
        *e++ = 0;
    } else {
        e = nullptr;
    }
    *save = e;
    return s;
}

wchar_t *wcsdup(const wchar_t *s) {
    size_t n = wcslen(s) + 1;
    wchar_t *d = malloc(n * sizeof(*d));
    return d ? wmemcpy(d, s, n) : nullptr;
}

wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n) {
    for (; n; n--, s++) {
        if (*s == c) {
            return (wchar_t *)s;
        }
    }
    return nullptr;
}

int wmemcmp(const wchar_t *a, const wchar_t *b, size_t n) {
    for (; n; n--, a++, b++) {
        if (*a != *b) {
            return *a < *b ? -1 : 1;
        }
    }
    return 0;
}

wchar_t *wmemcpy(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n) {
    return memcpy(d, s, n * sizeof(*d));
}

wchar_t *wmemmove(wchar_t *d, const wchar_t *s, size_t n) {
    return memmove(d, s, n * sizeof(*d));
}

wchar_t *wmemset(wchar_t *d, wchar_t c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        d[i] = c;
    }
    return d;
}
