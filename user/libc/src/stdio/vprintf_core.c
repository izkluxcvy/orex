#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <stdio_impl.h>

#ifndef CH
#define CH   char
#define WIDE 0
#define CORE __vprintf_core
#endif

/*
 * output
 */

static int out(struct sink *o, const char *p, size_t n) {
    if (!n) {
        return 0;
    }
    o->count += n;

#if WIDE
    wchar_t *w[64];
    while (n) {
        size_t k = n < 64 ? n : 64;
        for (size_t i = 0; i < k; i++) {
            w[i] = (unsigned char)p[i];
        }
        if (o->wwrite(o, w, k)) {
            return -1;
        }
        p += k;
        n -= k;
    }
    return 0;
#else
    return o->write(o, p, n);
#endif
}

static int out_ch(struct sink *o, const CH *p, size_t n) {
#if WIDE
    if (!n) {
        return 0;
    }
    o->count += n;
    return o->wwrite(o, p, n);
#else
    return out(o, p, n);
#endif
}

static const CH *find_pct(const CH *f) {
    for (; *f; f++) {
        if (*f == '%') {
            return f;
        }
    }
    return nullptr;
}

static size_t ch_len(const CH *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

static int pad(struct sink *o, char c, long n) {
    char block[64];
    memset(block, c, sizeof(block));
    for (; n > 0; n -= (long)sizeof(block)) {
        if (out(o, block,
                n < (long)sizeof(block) ? (size_t)n : sizeof(block))) {
            return -1;
        }
    }
    return 0;
}

/*
 * conversion specifications
 */

#define FL_MINUS 1
#define FL_PLUS  2
#define FL_SPACE 4
#define FL_HASH  8
#define FL_ZERO  16

enum { L_NONE, L_HH, L_H, L_L, L_LL, L_J, L_Z, L_T, L_BIGL };

enum { A_NONE, A_INT, A_LONG, A_PTR, A_DBL, A_LDBL };

union arg {
    uintmax_t i;
    double d;
    long double ld;
    void *p;
};

struct spec {
    int flags, width, prec, len;
    char conv;
    int pos, wpos, ppos;
};

static int number(const CH **f) {
    int n = 0;
    while (**f >= '0' && **f <= '9') {
        if (n < INT_MAX / 10) {
            n = n * 10 + (*(*f)++ - '0');
        } else {
            n = INT_MAX;
            (*f)++;
        }
    }
    return n;
}

static int position(const CH **f) {
    const CH *p = *f;
    int n = number(&p);
    if (n > 0 && *p == '$') {
        *f = p + 1;
        return n;
    }
    return 0;
}

static int parse(const CH **fp, struct spec *s) {
    const CH *f = *fp;
    *s = (struct spec){.prec = -1};
    s->pos = position(&f);
    for (;; f++) {
        int bit = *f == '-'    ? FL_MINUS
                  : *f == '+'  ? FL_PLUS
                  : *f == ' '  ? FL_SPACE
                  : *f == '#'  ? FL_HASH
                  : *f == '0'  ? FL_ZERO
                  : *f == '\'' ? -1
                               : 0;
        if (!bit) {
            break;
        }
        s->flags |= bit > 0 ? bit : 0;
    }
    if (*f == '*') {
        f++;
        s->wpos = position(&f);
        s->width = -2;
    } else {
        s->width = number(&f);
    }
    if (*f == '.') {
        f++;
        if (*f == '*') {
            f++;
            s->ppos = position(&f);
            s->prec = -2;
        } else {
            s->prec = number(&f);
        }
    }

    switch (*f) {
    case 'h':
        s->len = f[1] == 'h' ? (f++, L_HH) : L_H;
        f++;
        break;
    case 'l':
        s->len = f[1] == 'l' ? (f++, L_LL) : L_L;
        f++;
        break;
    case 'j':
        s->len = L_J;
        f++;
        break;
    case 'z':
        s->len = L_Z;
        f++;
        break;
    case 't':
        s->len = L_T;
        f++;
        break;
    case 'L':
        s->len = L_BIGL;
        f++;
        break;
    case 'w': {
        f++;
        if (*f == 'f') {
            f++;
        }
        int bits = number(&f);
        s->len = bits == 8 ? L_HH : bits == 16 ? L_H : bits == 64 ? L_L : 0;
        if (bits != 8 && bits != 16 && bits != 32 && bits != 64) {
            return -1;
        }
        break;
    }
    }
    s->conv = (unsigned)*f < 0x80 ? (char)*f : '?';
    if (!*f) {
        return -1;
    }
    *fp = f + 1;
    return 0;
}

static int arg_type(const struct spec *s) {
    switch (s->conv) {
    case 'd':
    case 'i':
    case 'o':
    case 'u':
    case 'x':
    case 'X':
    case 'b':
    case 'B':
        return s->len >= L_L && s->len <= L_T ? A_LONG : A_INT;
    case 'c':
        return A_INT;
    case 's':
    case 'p':
    case 'n':
        return A_PTR;
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
    case 'a':
    case 'A':
        return s->len == L_BIGL ? A_LDBL : A_DBL;
    default:
        return A_NONE;
    }
}

static void fetch(union arg *a, int type, va_list *ap) {
    switch (type) {
    case A_INT:
        a->i = (unsigned)va_arg(*ap, int);
        break;
    case A_LONG:
        a->i = va_arg(*ap, unsigned long);
        break;
    case A_PTR:
        a->p = va_arg(*ap, void *);
        break;
    case A_DBL:
        a->d = va_arg(*ap, double);
        break;
    case A_LDBL:
        a->ld = va_arg(*ap, long double);
        break;
    }
}

/*
 * integers
 */

static uintmax_t as_unsigned(uintmax_t v, int len) {
    switch (len) {
    case L_HH:
        return (unsigned char)v;
    case L_H:
        return (unsigned short)v;
    case L_NONE:
        return (unsigned)v;
    default:
        return v;
    }
}

static intmax_t as_signed(uintmax_t v, int len) {
    switch (len) {
    case L_HH:
        return (signed char)v;
    case L_H:
        return (short)v;
    case L_NONE:
        return (int)v;
    default:
        return (intmax_t)v;
    }
}

static int emit(struct sink *o, const struct spec *s, const char *prefix,
                long zeros, const char *digits, long ndig) {
    long plen = (long)strlen(prefix);
    long total = plen + zeros + ndig;
    long room = s->width > total ? s->width - total : 0;
    if (!(s->flags & FL_MINUS) && pad(o, ' ', room)) {
        return -1;
    }
    if (out(o, prefix, (size_t)plen) || pad(o, '0', zeros) ||
        out(o, digits, (size_t)ndig)) {
        return -1;
    }
    return (s->flags & FL_MINUS) ? pad(o, ' ', room) : 0;
}

static int fmt_int(struct sink *o, struct spec *s, uintmax_t raw) {
    char buf[72];
    char *end = buf + sizeof(buf);
    char *p = end;
    char conv = s->conv;
    int neg = 0;
    uintmax_t v;
    if (conv == 'd' || conv == 'i') {
        intmax_t sv = as_signed(raw, s->len);
        neg = sv < 0;
        v = neg ? -(uintmax_t)sv : (uintmax_t)sv;
    } else {
        v = as_unsigned(raw, s->len);
    }
    unsigned base = conv == 'o'                  ? 8
                    : conv == 'x' || conv == 'X' ? 16
                    : conv == 'b' || conv == 'B' ? 2
                                                 : 10;
    const char *digits = conv == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
    for (uintmax_t x = v; x; x /= base) {
        *--p = digits[x % base];
    }
    if (s->prec < 0 && v == 0) {
        *--p = '0';
    }
    long ndig = end - p;

    const char *prefix =
        neg                                                     ? "-"
        : (s->flags & FL_PLUS) && (conv == 'd' || conv == 'i')  ? "+"
        : (s->flags & FL_SPACE) && (conv == 'd' || conv == 'i') ? " "
                                                                : "";
    if ((s->flags & FL_HASH) && v) {
        prefix = conv == 'x'   ? "0x"
                 : conv == 'X' ? "0X"
                 : conv == 'b' ? "0b"
                 : conv == 'B' ? "0B"
                               : prefix;
    }
    long zeros = s->prec > ndig ? s->prec - ndig : 0;
    if (conv == 'o' && (s->flags & FL_HASH) && !zeros && (!ndig || *p != '0')) {
        zeros = 1;
    }
    if ((s->flags & FL_ZERO) && !(s->flags & FL_MINUS) && s->prec < 0) {
        long used = (long)strlen(prefix) + ndig;
        zeros = s->width > used ? s->width - used : 0;
    }
    return emit(o, s, prefix, zeros, p, ndig);
}

/*
 * floating point
 */

enum { FP_ZERO, FP_FINITE, FP_INF, FP_NAN };

struct fp {
    int sign, cls;
    uint64_t m; // value = m * 2^e
    int e;
    int bits; // 53 or 64
};

static struct fp decompose_d(double v) {
    uint64_t b;
    struct fp r = {.bits = 53};
    memcpy(&b, &v, sizeof(b));
    r.sign = (int)(b >> 63);
    int e = (int)(b >> 52) & 0x7ff;
    uint64_t f = b & ((1ULL << 52) - 1);
    if (e == 0x7ff) {
        r.cls = f ? FP_NAN : FP_INF;
    } else if (e == 0) {
        r.cls = f ? FP_FINITE : FP_ZERO;
        r.m = f;
        r.e = -1074;
    } else {
        r.cls = FP_FINITE;
        r.m = f | (1ULL << 52);
        r.e = e - 1075;
    }
    return r;
}

static struct fp decompose_ld(long double v) {
    struct {
        uint64_t m;
        uint16_t se;
    } b;
    struct fp r = {.bits = 64};
    memcpy(&b, &v, sizeof(b));
    r.sign = b.se >> 15;
    int e = b.se & 0x7fff;
    if (e == 0x7fff) {
        r.cls = (b.m << 1) ? FP_NAN : FP_INF;
    } else if (!b.m) {
        r.cls = FP_ZERO;
    } else {
        r.cls = FP_FINITE;
        r.m = b.m;
        r.e = (e ? e : 1) - 16383 - 63;
    }
    return r;
}

#define BILLION 1'000'000'000U

static size_t digits_bound(int e) {
    if (e >= 0) {
        return (size_t)(64 + e) * 30103 / 100000 + 3;
    } else {
        return 20 + (size_t)(-e) * 69898 / 100000 + 3;
    }
}

#define SMALL_DIGITS 800
#define SMALL_LIMBS  (SMALL_DIGITS / 9 + 4)

static int exact_digits(uint64_t m, int e, char *d, uint32_t *limb, int *x) {
    int n = 0;
    for (; m; m /= BILLION) {
        limb[n++] = (uint32_t)(m % BILLION);
    }
    int frac = 0;
    for (int k = e; k > 0;) {
        int step = k > 29 ? 29 : k;
        uint64_t carry = 0;
        for (int i = 0; i < n; i++) {
            uint64_t t = ((uint64_t)limb[i] << step) + carry;
            limb[i] = (uint32_t)(t % BILLION);
            carry = t / BILLION;
        }
        if (carry) {
            limb[n++] = (uint32_t)carry;
        }
        k -= step;
    }
    for (int k = -e; k > 0;) {
        int step = k > 13 ? 13 : k;
        uint32_t mul = 1;
        for (int i = 0; i < step; i++) {
            mul *= 5;
        }
        uint64_t carry = 0;
        for (int i = 0; i < n; i++) {
            uint64_t t = (uint64_t)limb[i] * mul + carry;
            limb[i] = (uint32_t)(t % BILLION);
            carry = t / BILLION;
        }
        while (carry) {
            limb[n++] = (uint32_t)(carry % BILLION);
            carry /= BILLION;
        }
        k -= step;
        frac += step;
    }

    int len = 0;
    for (int i = n - 1; i >= 0; i--) {
        char nine[9];
        uint32_t v = limb[i];
        for (int j = 8; j >= 0; j--, v /= 10) {
            nine[j] = (char)('0' + v % 10);
        }
        int j = 0;
        if (i == n - 1) {
            while (j < 8 && nine[j] == '0') {
                j++;
            }
        }
        memcpy(d + len, nine + j, (size_t)(9 - j));
        len += 9 - j;
    }
    *x = len - frac - 1;
    while (len > 1 && d[len - 1] == '0') {
        len--;
    }
    return len;
}

static int round_to(char *d, int n, int keep, int *x) {
    if (keep >= n) {
        return n;
    }
    int up = 0;
    if (keep >= 0) {
        if (d[keep] != '5') {
            up = d[keep] > '5';
        } else {
            for (int i = keep + 1; i < n && !up; i++) {
                up = d[i] != '0';
            }
            if (!up && keep > 0) {
                up = (d[keep - 1] - '0') & 1;
            }
        }
    }
    if (keep <= 0) {
        if (!up) {
            return 0;
        }
        d[0] = '1';
        *x += 1 - keep;
        return 1;
    }
    if (up) {
        int i = keep - 1;
        while (i >= 0 && d[i] == '9') {
            i--;
        }
        if (i < 0) {
            d[0] = '1';
            (*x)++;
            return 1;
        }
        d[i]++;
        keep = i + 1;
    }
    while (keep > 1 && d[keep - 1] == '0') {
        keep--;
    }
    return keep;
}

struct chunk {
    struct sink *o;
    char buf[256];
    size_t n;
    int err;
};

static void put(struct chunk *c, char ch) {
    c->buf[c->n++] = ch;
    if (c->n == sizeof(c->buf)) {
        c->err |= out(c->o, c->buf, c->n);
        c->n = 0;
    }
}

static void put_zeros(struct chunk *c, long n) {
    while (n-- > 0) {
        put(c, '0');
    }
}

static int put_done(struct chunk *c) {
    c->err |= out(c->o, c->buf, c->n);
    return c->err ? -1 : 0;
}

static int emit_float(struct sink *o, const struct spec *s, const char *prefix,
                      long len, int (*body)(struct chunk *, const void *),
                      const void *ctx, int zero_ok) {
    const char *sign = prefix;
    long slen = (long)strlen(sign);
    long total = slen + len;
    long room = s->width > total ? s->width - total : 0;
    int zpad = zero_ok && (s->flags & FL_ZERO) && !(s->flags & FL_MINUS);
    struct chunk c = {.o = o};
    if (!(s->flags & FL_MINUS) && !zpad && pad(o, ' ', room)) {
        return -1;
    }
    if (out(o, sign, (size_t)slen) || (zpad && pad(o, '0', room))) {
        return -1;
    }
    body(&c, ctx);
    if (put_done(&c)) {
        return -1;
    }
    return (s->flags & FL_MINUS) ? pad(o, ' ', room) : 0;
}

struct decimal {
    const char *d;
    int n, x;
    int style; // 'f' or 'e'
    int frac;
    int point;
    char e; // 'e' or 'E'
};

static int digit(const struct decimal *v, long i) {
    if (i >= 0 && i < v->n) {
        return v->d[i];
    }
    return '0';
}

static int body_decimal(struct chunk *c, const void *ctx) {
    const struct decimal *v = ctx;
    // f style: 123.456
    if (v->style == 'f') {
        long ip = v->x >= 0 ? v->x + 1 : 1;
        for (long i = 0; i < ip; i++) {
            put(c, (char)(v->x >= 0 ? digit(v, i) : '0'));
        }
        if (v->point) {
            put(c, '.');
        }
        long first = v->x + 1;
        long j = 0;
        for (; j < v->frac && first + j < v->n; j++) {
            put(c, (char)digit(v, first + j));
        }
        put_zeros(c, v->frac - j);
        return 0;
    }

    // e style: 1.23456e+02
    put(c, (char)digit(v, 0));
    if (v->point) {
        put(c, '.');
    }
    long j = 0;
    for (; j < v->frac && 1 + j < v->n; j++) {
        put(c, (char)digit(v, 1 + j));
    }
    put_zeros(c, v->frac - j);
    int x = v->n ? v->x : 0;
    char ex[8];
    char *p = ex + sizeof(ex);
    for (int a = x < 0 ? -x : x, k = 0; a || k < 2; a /= 10, k++) {
        *--p = (char)('0' + a % 10);
    }
    put(c, v->e);
    put(c, x < 0 ? '-' : '+');
    while (p < ex + sizeof(ex)) {
        put(c, *p++);
    }
    return 0;
}

static long decimal_len(const struct decimal *v) {
    long len = v->point + v->frac;
    if (v->style == 'f') {
        return len + (v->x >= 0 ? v->x + 1 : 1);
    }

    int x = v->n ? v->x : 0;
    int ex = 2;
    for (int a = (x < 0 ? -x : x) / 100; a; a /= 100) {
        ex++;
    }
    return len + 1 + 2 + ex;
}

static int fmt_decimal(struct sink *o, const struct spec *s, const char *sign,
                       struct fp *f) {
    char small[SMALL_DIGITS];
    char *d = small;
    uint32_t small_limbs[SMALL_LIMBS];
    uint32_t *limbs = small_limbs;
    void *heap = nullptr;
    int n = 0, x = 0;
    if (f->cls == FP_FINITE) {
        size_t need = digits_bound(f->e);
        if (need > SMALL_DIGITS) {
            heap = malloc(need + (need / 9 + 4) * sizeof(uint32_t));
            if (!heap) {
                return -1;
            }
            limbs = heap;
            d = (char *)(limbs + need / 9 + 4);
        }
        n = exact_digits(f->m, f->e, d, limbs, &x);
    }
    char conv = s->conv;
    char lower = (char)(conv | 0x20);
    int prec = s->prec < 0 ? 6 : s->prec;
    struct decimal v = {.d = d, .e = conv == 'E' || conv == 'G' ? 'E' : 'e'};

    if (lower == 'g') {
        int p = prec ? prec : 1;
        n = round_to(d, n, p, &x);
        if (!n) {
            x = 0;
        }
        if (p > x && x >= -4) {
            v.style = 'f';
            v.frac = p - 1 - x;
        } else {
            v.style = 'e';
            v.frac = p - 1;
        }
        if (!(s->flags & FL_HASH)) {
            int last = n - 1;
            while (last >= 0 && d[last] == '0') {
                last--;
            }
            int have = v.style == 'f' ? last - x : last;
            v.frac = have < 0 ? 0 : have < v.frac ? have : v.frac;
        }
    } else if (lower == 'e') {
        n = round_to(d, n, prec + 1, &x);
        v.style = 'e';
        v.frac = prec;
    } else {
        n = round_to(d, n, x + 1 + prec, &x);
        v.style = 'f';
        v.frac = prec;
    }
    v.n = n;
    v.x = n ? x : 0;
    v.point = v.frac > 0 || (s->flags & FL_HASH);
    if (v.style == 'f' && !n) {
        v.x = 0;
    }
    int r = emit_float(o, s, sign, decimal_len(&v), body_decimal, &v, 1);
    free(heap);
    return r;
}

struct hex {
    int lead;
    uint64_t frac;
    int nd;
    int point;
    int exp;
    int upper;
};

static int body_hex(struct chunk *c, const void *ctx) {
    const struct hex *h = ctx;
    const char *dig = h->upper ? "0123456789ABCDEF" : "0123456789abcdef";
    put(c, dig[h->lead]);
    if (h->point) {
        put(c, '.');
    }
    uint64_t f = h->frac;
    for (int i = 0; i < h->nd; i++, f <<= 4) {
        put(c, i < 16 ? dig[f >> 60] : '0');
    }
    put(c, h->upper ? 'P' : 'p');
    put(c, h->exp < 0 ? '-' : '+');
    char ex[8];
    char *p = ex + sizeof(ex);
    for (int a = h->exp < 0 ? -h->exp : h->exp, k = 0; a || !k; a /= 10, k++) {
        *--p = (char)('0' + a % 10);
    }
    while (p < ex + sizeof(ex)) {
        put(c, *p++);
    }
    return 0;
}

static int fmt_hex(struct sink *o, const struct spec *s, const char *sign,
                   struct fp *f) {
    struct hex h = {.upper = s->conv == 'A'};
    int max;
    if (f->cls == FP_ZERO) {
        max = 0;
    } else {
        uint64_t m = f->m;
        int e = f->e + f->bits - 1;
        while (!(m >> (f->bits - 1))) {
            m <<= 1;
            e--;
        }
        h.lead = 1;
        h.exp = e;
        h.frac = m << (65 - f->bits);
        max = (f->bits - 1 + 3) / 4;
    }
    int nd = s->prec < 0 ? max : s->prec;
    if (s->prec >= 0 && nd < max) {
        int drop = 64 - 4 * nd;
        uint64_t keep = drop == 64 ? 0 : h.frac >> drop;
        uint64_t rest = drop == 64 ? h.frac : h.frac << (64 - drop);
        uint64_t half = 1ULL << 63;
        int odd = drop == 64 ? h.lead & 1 : (int)(keep & 1);
        if (rest > half || (rest == half && odd)) {
            keep++;
            if (drop == 64 || keep >> (64 - drop)) {
                h.lead++;
                keep = 0;
            }
        }
        h.frac = drop == 64 ? 0 : keep << drop;
    }
    if (s->prec < 0) {
        while (nd > 0 && !((h.frac >> (64 - 4 * nd)) & 15)) {
            nd--;
        }
    }
    h.nd = nd;
    h.point = nd > 0 || (s->flags & FL_HASH);
    long exd = 1;
    for (int a = (h.exp < 0 ? -h.exp : h.exp) / 10; a; a /= 10) {
        exd++;
    }
    long len = 1 + h.point + nd + 2 + exd;
    char pre[4];
    strcpy(pre, sign);
    strcat(pre, h.upper ? "0X" : "0x");
    return emit_float(o, s, pre, len, body_hex, &h, 1);
}

[[gnu::noinline]] static int fmt_float(struct sink *o, struct spec *s,
                                       union arg *a) {
    struct fp f = s->len == L_BIGL ? decompose_ld(a->ld) : decompose_d(a->d);
    const char *sign = f.sign                  ? "-"
                       : (s->flags & FL_PLUS)  ? "+"
                       : (s->flags & FL_SPACE) ? " "
                                               : "";
    if (f.cls == FP_INF || f.cls == FP_NAN) {
        int upper = s->conv >= 'A' && s->conv <= 'Z';
        const char *word =
            f.cls == FP_INF ? (upper ? "INF" : "inf") : (upper ? "NAN" : "nan");
        struct spec t = *s;
        t.prec = -1;
        return emit(o, &t, sign, 0, word, 3);
    }
    if ((s->conv | 0x20) == 'a') {
        return fmt_hex(o, s, sign, &f);
    }
    return fmt_decimal(o, s, sign, &f);
}

/*
 * strings
 */

static int fmt_str(struct sink *o, struct spec *s, const char *str, long len) {
    struct spec t = *s;
    t.prec = -1;
    return emit(o, &t, "", 0, str, len);
}

#if WIDE
static int put_wide(struct sink *o, const struct spec *s, const wchar_t *ws,
                    long len) {
    long room = s->width > len ? s->width - len : 0;
    if (!(s->flags & FL_MINUS) && pad(o, ' ', room)) {
        return -1;
    }
    if (out_ch(o, ws, (size_t)len)) {
        return -1;
    }
    return (s->flags & FL_MINUS) ? pad(o, ' ', room) : 0;
}

static int fmt_wide(struct sink *o, struct spec *s, const wchar_t *ws) {
    long len = (long)(s->prec >= 0 ? wcsnlen(ws, (size_t)s->prec) : wcslen(ws));
    return put_wide(o, s, ws, len);
}

static int fmt_mb(struct sink *o, struct spec *s, const char *str) {
    mbstate_t st = {0};
    const char *p = str;
    long len = 0;
    for (; s->prec < 0 || len < s->prec; len++) {
        size_t k = mbrtowc(nullptr, p, 4, &st);
        if (k == (size_t)-1 || k == (size_t)-2) {
            errno = EILSEQ;
            return -1;
        }
        if (!k) {
            break;
        }
        p += k;
    }
    long room = s->width > len ? s->width - len : 0;
    if (!(s->flags & FL_MINUS) && pad(o, ' ', room)) {
        return -1;
    }
    wchar_t w[64];
    p = str;
    for (long done = 0; done < len;) {
        long k = 0;
        for (; k < 64 && done + k < len; k++) {
            p += mbrtowc(&w[k], p, 4, &st);
        }
        if (out_ch(o, w, (size_t)k)) {
            return -1;
        }
        done += k;
    }
    return (s->flags & FL_MINUS) ? pad(o, ' ', room) : 0;
}
#else
static int fmt_wide(struct sink *o, struct spec *s, const wchar_t *ws) {
    mbstate_t st = {0};
    long len = 0;
    char b[4];
    for (const wchar_t *p = ws; *p; p++) {
        size_t k = wcrtomb(b, *p, &st);
        if (k == (size_t)-1) {
            return -1;
        }
        if (s->prec >= 0 && len + (long)k > s->prec) {
            break;
        }
        len += (long)k;
    }
    long room = s->width > len ? s->width - len : 0;
    if (!(s->flags & FL_MINUS) && pad(o, ' ', room)) {
        return -1;
    }
    char buf[256];
    long n = 0;
    for (long done = 0; done < len; ws++) {
        size_t k = wcrtomb(buf + n, *ws, &st);
        n += (long)k;
        done += (long)k;
        if (n > (long)sizeof(buf) - 4 || done == len) {
            if (out(o, buf, (size_t)n)) {
                return -1;
            }
            n = 0;
        }
    }
    return (s->flags & FL_MINUS) ? pad(o, ' ', room) : 0;
}
#endif

/*
 * main loop
 */

static int convert(struct sink *o, struct spec *s, union arg *a, int saved) {
    switch (s->conv) {
    case 'd':
    case 'i':
    case 'o':
    case 'u':
    case 'x':
    case 'X':
    case 'b':
    case 'B':
        return fmt_int(o, s, a->i);
#if WIDE
    case 'c': {
        wint_t c = s->len == L_L ? (wint_t)a->i : btowc((int)a->i);
        wchar_t wc = (wchar_t)c;
        struct spec t = *s;
        t.prec = -1;
        if (c == WEOF) {
            errno = EILSEQ;
            return -1;
        }
        return put_wide(o, &t, &wc, 1);
    }
    case 's':
        if (s->len == L_L) {
            return fmt_wide(o, s, a->p ? a->p : L"(null)");
        }
        return fmt_mb(o, s, a->p ? a->p : "(null)");
#else
    case 'c': {
        if (s->len == L_L) {
            wchar_t w[2] = {(wchar_t)a->i, 0};
            struct spec t = *s;
            t.prec = -1;
            if (!w[0]) {
                char z = 0;
                return fmt_str(o, s, &z, 1);
            }
            return fmt_wide(o, &t, w);
        }
        char c = (char)a->i;
        return fmt_str(o, s, &c, 1);
    }
    case 's':
        if (s->len == L_L) {
            return fmt_wide(o, s, a->p ? a->p : L"(null)");
        } else {
            const char *str = a->p ? a->p : "(null)";
            long len = (long)(s->prec >= 0 ? strnlen(str, (size_t)s->prec)
                                           : strlen(str));
            return fmt_str(o, s, str, len);
        }
#endif
    case 'p':
        if (!a->p) {
            return fmt_str(o, s, "(nil)", 5);
        } else {
            struct spec t = *s;
            t.conv = 'x';
            t.len = L_L;
            t.flags |= FL_HASH;
            return fmt_int(o, &t, (uintptr_t)a->p);
        }
    case 'n':
        switch (s->len) {
        case L_HH:
            *(signed char *)a->p = (signed char)o->count;
            break;
        case L_H:
            *(short *)a->p = (short)o->count;
            break;
        case L_NONE:
            *(int *)a->p = (int)o->count;
            break;
        default:
            *(long *)a->p = (long)o->count;
        }
        return 0;
    case 'm': {
        const char *msg = strerror(saved);
        return fmt_str(o, s, msg, (long)strlen(msg));
    }
    case '%':
        return out(o, "%", 1);
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
    case 'a':
    case 'A':
        return fmt_float(o, s, a);
    default:
        errno = EINVAL;
        return -1;
    }
}

static void apply_stars(struct spec *s, int width, int prec) {
    if (s->width == -2) {
        s->width = width;
        if (s->width < 0) {
            s->flags |= FL_MINUS;
            s->width = s->width == INT_MIN ? INT_MAX : -s->width;
        }
    }
    if (s->prec == -2) {
        s->prec = prec < 0 ? -1 : prec;
    }
}

static int positional(struct sink *o, const CH *fmt, va_list ap, int saved) {
    int types[NL_ARGMAX + 1] = {0};
    union arg args[NL_ARGMAX + 1];
    int max = 0;
    struct spec s;
    for (const CH *f = fmt; *f;) {
        if (*f++ != '%') {
            continue;
        }
        if (*f == '%') {
            f++;
            continue;
        }
        if (parse(&f, &s) != 0 || !s.pos || (s.width == -2 && !s.wpos) ||
            (s.prec == -2 && !s.ppos) || s.pos > NL_ARGMAX ||
            s.wpos > NL_ARGMAX || s.ppos > NL_ARGMAX) {
            errno = EINVAL;
            return -1;
        }
        types[s.pos] = arg_type(&s);
        if (s.wpos) {
            types[s.wpos] = A_INT;
        }
        if (s.ppos) {
            types[s.ppos] = A_INT;
        }
        int hi = s.pos > s.wpos ? s.pos : s.wpos;
        hi = hi > s.ppos ? hi : s.ppos;
        max = hi > max ? hi : max;
    }
    va_list aq;
    va_copy(aq, ap);
    for (int i = 1; i <= max; i++) {
        if (!types[i]) {
            va_end(aq);
            errno = EINVAL;
            return -1;
        }
        fetch(&args[i], types[i], &aq);
    }
    va_end(aq);

    for (const CH *f = fmt; *f;) {
        const CH *lit = find_pct(f);
        size_t n = lit ? (size_t)(lit - f) : ch_len(f);
        if (out_ch(o, f, n)) {
            return -1;
        }
        if (!lit) {
            break;
        }
        f = lit + 1;
        if (*f == '%') {
            if (out(o, "%", 1)) {
                return -1;
            }
            f++;
            continue;
        }
        parse(&f, &s);
        apply_stars(&s, s.wpos ? (int)args[s.wpos].i : 0,
                    s.ppos ? (int)args[s.ppos].i : 0);
        if (convert(o, &s, &args[s.pos], saved)) {
            return -1;
        }
    }
    return 0;
}

static int numbered(const CH *fmt) {
    for (const CH *f = fmt; (f = find_pct(f));) {
        f++;
        if (*f == '%') {
            f++;
            continue;
        }
        struct spec s;
        return parse(&f, &s) == 0 && s.pos;
    }
    return 0;
}

int CORE(struct sink *o, const CH *fmt, va_list ap) {
    int saved = errno;
    if (numbered(fmt)) {
        return positional(o, fmt, ap, saved);
    }

    va_list aq;
    va_copy(aq, ap);
    for (const CH *f = fmt; *f;) {
        const CH *lit = find_pct(f);
        size_t n = lit ? (size_t)(lit - f) : ch_len(f);
        if (out_ch(o, f, n)) {
            goto fail;
        }
        if (!lit) {
            break;
        }
        f = lit + 1;
        struct spec s;
        if (parse(&f, &s) != 0 || s.pos || s.wpos || s.ppos) {
            errno = EINVAL;
            goto fail;
        }
        int width = s.width == -2 ? va_arg(aq, int) : 0;
        int prec = s.prec == -2 ? va_arg(aq, int) : 0;
        apply_stars(&s, width, prec);
        union arg a = {0};
        fetch(&a, arg_type(&s), &aq);
        if (convert(o, &s, &a, saved)) {
            goto fail;
        }
    }
    va_end(aq);
    return 0;

fail:
    va_end(aq);
    return -1;
}
