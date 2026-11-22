#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <stdio_impl.h>

#ifndef CH
#define CH       char
#define WIDE     0
#define CORE     __vscanf_core
#define SPACE(c) ((c) >= 0 && (c) < 256 && isspace(c))
#endif

#define DIGIT(c)  ((c) >= '0' && (c) <= '9')
#define ALPHA(c)  (((c) | 0x20) >= 'a' && ((c) | 0x20) <= 'z')
#define XDIGIT(c) (DIGIT(c) || (((c) | 0x20) >= 'a' && ((c) | 0x20) <= 'f'))

static int get(struct source *in) {
    int c = in->get(in);
    in->eof = c == EOF;
    if (c != EOF) {
        in->count++;
    }
    return c;
}

static void unget(struct source *in, int c) {
    if (c != EOF) {
        in->unget(in, c);
        in->count--;
    }
}

static void skip_space(struct source *in) {
    int c;
    while ((c = get(in)) != EOF && SPACE(c)) {
    }
    unget(in, c);
}

struct text {
    char *buf;
    size_t n, cap;
};

static int text_add(struct text *t, int c) {
    if (t->n + 2 > t->cap) {
        size_t cap = t->cap ? 2 * t->cap : 64;
        char *grow = realloc(t->buf, cap);
        if (!grow) {
            return -1;
        }
        t->buf = grow;
        t->cap = cap;
    }
    t->buf[t->n++] = (char)c;
    t->buf[t->n] = '\0';
    return 0;
}

enum {
    N_START,
    N_SIGNED,
    N_ZERO,
    N_HEX0,
    N_INT,
    N_DOT0,
    N_FRAC,
    N_EXP,
    N_EXPSIGN,
    N_EXPDIG,
    N_WORD,
    N_PAREN,
    N_DONE,
    N_END
};

static int gather_float(struct source *in, struct text *t, long width) {
    int st = N_START, hex = 0, valid = 0, wi = 0;
    const char *word = nullptr;
    for (long k = 0; width < 0 || k < width; k++) {
        int c = get(in);
        int next = N_END;
        int lc = c | 0x20;
        int digit = hex ? XDIGIT(c) : DIGIT(c);
        switch (st) {
        case N_START:
            if (c == '+' || c == '-') {
                next = N_SIGNED;
                break;
            }
            [[fallthrough]];
        case N_SIGNED:
            if (c == '0') {
                next = N_ZERO;
            } else if (DIGIT(c)) {
                next = N_INT;
            } else if (c == '.') {
                next = N_DOT0;
            } else if (lc == 'i' || lc == 'n') {
                word = lc == 'i' ? "infinity" : "nan";
                wi = 1;
                next = N_WORD;
            }
            break;
        case N_ZERO:
            next = lc == 'x'   ? (hex = 1, N_HEX0)
                   : digit     ? N_INT
                   : c == '.'  ? N_FRAC
                   : lc == 'e' ? N_EXP
                               : N_END;
            break;
        case N_HEX0:
            next = digit ? N_INT : c == '.' ? N_DOT0 : N_END;
            break;
        case N_INT:
            next = digit                     ? N_INT
                   : c == '.'                ? N_FRAC
                   : lc == (hex ? 'p' : 'e') ? N_EXP
                                             : N_END;
            break;
        case N_DOT0:
            next = digit ? N_FRAC : N_END;
            break;
        case N_FRAC:
            next = digit ? N_FRAC : lc == (hex ? 'p' : 'e') ? N_EXP : N_END;
            break;
        case N_EXP:
            next = c == '+' || c == '-' ? N_EXPSIGN
                   : DIGIT(c)           ? N_EXPDIG
                                        : N_END;
            break;
        case N_EXPSIGN:
        case N_EXPDIG:
            next = DIGIT(c) ? N_EXPDIG : N_END;
            break;
        case N_WORD:
            if (word[wi] && lc == word[wi]) {
                wi++;
                next = N_WORD;
            } else if (word[0] == 'n' && !word[wi] && c == '(') {
                next = N_PAREN;
            }
            break;
        case N_PAREN:
            next = DIGIT(c) || ALPHA(c) || c == '_' ? N_PAREN
                   : c == ')'                       ? N_DONE
                                                    : N_END;
            break;
        }

        if (next == N_END) {
            unget(in, c);
            break;
        }
        if (text_add(t, c) != 0) {
            return 0;
        }
        st = next;
        valid = st == N_ZERO || st == N_INT || st == N_FRAC || st == N_EXPDIG ||
                st == N_DONE ||
                (st == N_WORD && (wi == 3 || (word[0] == 'i' && wi == 8)));
        if (st == N_DONE) {
            break;
        }
    }
    return valid;
}

static int gather_int(struct source *in, struct text *t, long width,
                      int *base) {
    int digits = 0, st = N_START;
    for (long k = 0; width < 0 || k < width; k++) {
        int c = get(in);
        int ok = 0;
        int lc = c | 0x20;
        if (st == N_START && (c == '+' || c == '-')) {
            ok = 1;
            st = N_SIGNED;
        } else if (st <= N_SIGNED && c == '0' && (*base == 0 || *base == 16)) {
            ok = 1;
            digits = 1;
            st = N_ZERO;
        } else if (st == N_ZERO && lc == 'x') {
            ok = 1;
            *base = 16;
            st = N_INT;
            digits = 0;
        } else {
            if (st == N_ZERO && !*base) {
                *base = 8;
            }
            if (!*base) {
                *base = 10;
            }
            int v = DIGIT(c) ? c - '0' : ALPHA(c) ? lc - 'a' + 10 : 99;
            if (v < *base) {
                ok = 1;
                digits++;
                st = N_INT;
            }
        }
        if (!ok) {
            unget(in, c);
            break;
        }
        if (text_add(t, c) != 0) {
            return 0;
        }
    }
    if (!*base) {
        *base = st == N_ZERO ? 8 : 10;
    }
    return digits > 0 || st == N_ZERO;
}

/*
 * storing
 */

enum { L_NONE, L_HH, L_H, L_L, L_LL, L_J, L_Z, L_T, L_BIGL };

static void store_int(void *p, int len, uintmax_t v) {
    switch (len) {
    case L_HH:
        *(char *)p = (char)v;
        break;
    case L_H:
        *(short *)p = (short)v;
        break;
    case L_NONE:
        *(int *)p = (int)v;
        break;
    default:
        *(long *)p = (long)v;
    }
}

struct args {
    va_list ap;
    void *pos[NL_ARGMAX + 1];
    int numbered;
};

static void *next_arg(struct args *a, int pos) {
    return a->numbered ? (pos > 0 && pos <= NL_ARGMAX ? a->pos[pos] : nullptr)
                       : va_arg(a->ap, void *);
}

struct scanset {
    const CH *b, *e;
    int neg;
};

static const CH *scanset(const CH *f, struct scanset *set) {
    set->neg = *f == '^';
    f += set->neg;
    set->b = f;
    if (*f == ']') {
        f++;
    }
    while (*f && *f != ']') {
        f++;
    }
    set->e = f;
    return *f ? f + 1 : f;
}

static int in_set(const struct scanset *set, int c) {
    int hit = 0;
    for (const CH *f = set->b; f < set->e && !hit; f++) {
        unsigned lo = (unsigned)*f;
        if (f + 2 < set->e && f[1] == '-' && (unsigned)f[2] >= lo) {
            hit = (unsigned)c >= lo && (unsigned)c <= (unsigned)f[2];
            f += 2;
        } else {
            hit = (unsigned)c == lo;
        }
    }
    return hit != set->neg;
}

static const CH *find_pct(const CH *f) {
    for (; *f; f++) {
        if (*f == '%') {
            return f;
        }
    }
    return nullptr;
}

struct store {
    char *p;
    size_t n, cap, size;
    int grow, ok;
};

static void keep(struct store *st, const void *v) {
    if (st->grow && st->ok && st->n + 1 >= st->cap) {
        char *g = realloc(st->p, (st->cap *= 2) * st->size + st->size);
        if (!g) {
            st->ok = 0;
            return;
        }
        st->p = g;
    }
    if (st->p && st->ok) {
        memcpy(st->p + st->n * st->size, v, st->size);
    }
    st->n++;
}

/*
 * main loop
 */

int CORE(struct source *in, const CH *fmt, va_list ap) {
    struct args a = {.numbered = 0};
    va_copy(a.ap, ap);

    for (const CH *f = fmt; (f = find_pct(f));) {
        f++;
        int n = 0;
        while (DIGIT(*f)) {
            n = n * 10 + (*f++ - '0');
        }
        if (*f == '$' && n > 0 && n <= NL_ARGMAX) {
            a.numbered = n > a.numbered ? n : a.numbered;
        }
    }
    if (a.numbered) {
        for (int i = 1; i <= a.numbered; i++) {
            a.pos[i] = va_arg(a.ap, void *);
        }
    }

    int assigned = 0, converted = 0, failed_input = 0;
    struct text t = {0};
    for (const CH *f = fmt; *f;) {
        if (SPACE(WIDE ? (int)*f : (unsigned char)*f)) {
            skip_space(in);
            f++;
            continue;
        }
        if (*f != '%' || f[1] == '%') {
            if (*f == '%') {
                skip_space(in);
                f++;
            }
            int c = get(in);
            if (c != (WIDE ? (int)*f : (unsigned char)*f)) {
                unget(in, c);
                failed_input = c == EOF;
                break;
            }
            f++;
            continue;
        }

        f++;
        int pos = 0;
        if (DIGIT(*f)) {
            const CH *p = f;
            int n = 0;
            while (DIGIT(*p)) {
                n = n * 10 + (*p++ - '0');
            }
            if (*p == '$') {
                pos = n;
                f = p + 1;
            }
        }
        int suppress = *f == '*';
        f += suppress;
        long width = -1;
        if (DIGIT(*f)) {
            width = 0;
            while (DIGIT(*f)) {
                width = width < INT_MAX ? width * 10 + (*f - '0') : width;
                f++;
            }
        }
        int alloc = *f == 'm';
        f += alloc;
        int len = L_NONE;
        switch (*f) {
        case 'h':
            len = f[1] == 'h' ? (f++, L_HH) : L_H;
            f++;
            break;
        case 'l':
            len = f[1] == 'l' ? (f++, L_LL) : L_L;
            f++;
            break;
        case 'j':
            len = L_J;
            f++;
            break;
        case 'z':
            len = L_Z;
            f++;
            break;
        case 't':
            len = L_T;
            f++;
            break;
        case 'L':
            len = L_BIGL;
            f++;
            break;
        }
        char conv = (unsigned)*f < 0x80 ? (char)*f : '?';
        f += !!*f;
        if (conv == 'C' || conv == 'S') {
            conv = conv == 'C' ? 'c' : 's';
            len = L_L;
        }

        if (conv == 'n') {
            void *p = suppress ? nullptr : next_arg(&a, pos);
            if (p) {
                store_int(p, len, in->count);
            }
            continue;
        }
        if (conv != 'c' && conv != '[') {
            skip_space(in);
        }
        void *dst = suppress ? nullptr : next_arg(&a, pos);
        if (conv == 'c' || conv == 's' || conv == '[') {
            struct scanset set;
            if (conv == '[') {
                f = scanset(f, &set);
            }
            long max = width < 0 ? (conv == 'c' ? 1 : -1) : width;
            struct store st = {
                .p = dst, .size = len == L_L ? sizeof(wchar_t) : 1, .ok = 1};
            if (alloc && dst) {
                st.cap = conv == 'c' && max > 0 ? (size_t)max : 32;
                st.p = malloc(st.cap * st.size + st.size);
                st.grow = 1;
                st.ok = !!st.p;
            }
            mbstate_t mbs = {0};
            int c = EOF, bad = 0;
            long n = 0;
            while (max < 0 || n < max) {
                c = get(in);
                if (c == EOF || (conv == 's' && SPACE(c)) ||
                    (conv == '[' && !in_set(&set, c))) {
                    unget(in, c);
                    break;
                }
                n++;
#if WIDE
                if (st.size == 1) {
                    char b[4];
                    size_t k = wcrtomb(b, (wchar_t)c, &mbs);
                    if (k == (size_t)-1) {
                        bad = 1;
                        break;
                    }
                    for (size_t i = 0; i < k; i++) {
                        keep(&st, &b[i]);
                    }
                } else {
                    wchar_t w = (wchar_t)c;
                    keep(&st, &w);
                }
#else
                if (st.size == 1) {
                    char b = (char)c;
                    keep(&st, &b);
                } else {
                    char b = (char)c;
                    wchar_t w;
                    size_t k = mbrtowc(&w, &b, 1, &mbs);
                    if (k == (size_t)-1) {
                        bad = 1;
                        break;
                    }
                    if (k != (size_t)-2) {
                        keep(&st, &w);
                    }
                }
#endif
            }
            if (bad || !st.ok || !n || (conv == 'c' && n < max) ||
                !mbsinit(&mbs)) {
                if (st.grow) {
                    free(st.p);
                }
                failed_input = c == EOF && !n;
                if (bad) {
                    errno = EILSEQ;
                }
                break;
            }
            if (st.p && conv != 'c') {
                memset(st.p + st.n * st.size, 0, st.size);
            }
            if (alloc && dst) {
                *(char **)dst = st.p;
            }
        } else if (strchr("diouxXbp", conv)) {
            int base = conv == 'd' || conv == 'u' ? 10
                       : conv == 'o'              ? 8
                       : conv == 'b'              ? 2
                       : conv == 'i'              ? 0
                                                  : 16;
            t.n = 0;
            if (!gather_int(in, &t, width, &base)) {
                failed_input = !t.n && in->eof;
                break;
            }
            uintmax_t v = conv == 'd' || conv == 'i'
                              ? (uintmax_t)strtoimax(t.buf, nullptr, base)
                              : strtoumax(t.buf, nullptr, base);
            if (dst) {
                if (conv == 'p') {
                    *(void **)dst = (void *)(uintptr_t)v;
                } else {
                    store_int(dst, len, v);
                }
            }
        } else if (strchr("aefgAEFG", conv)) {
            t.n = 0;
            if (!gather_float(in, &t, width)) {
                failed_input = !t.n && in->eof;
                break;
            }
            char *end;
            long double v = __floatscan(t.buf, &end,
                                        len == L_BIGL ? &__float_ld
                                        : len == L_L  ? &__float_d
                                                      : &__float_f);
            if (dst) {
                if (len == L_BIGL) {
                    *(long double *)dst = v;
                } else if (len == L_L) {
                    *(double *)dst = (double)v;
                } else {
                    *(float *)dst = (float)v;
                }
            }
        } else {
            break;
        }
        converted++;
        if (dst) {
            assigned++;
        }
    }
    free(t.buf);
    va_end(a.ap);
    return failed_input && !converted ? EOF : assigned;
}
