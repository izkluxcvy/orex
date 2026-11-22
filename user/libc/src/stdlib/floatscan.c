#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <stdio_impl.h>

const struct float_kind __float_f = {24, -126, 127};
const struct float_kind __float_d = {53, -1022, 1023};
const struct float_kind __float_ld = {64, -16382, 16383};

typedef unsigned __int128 u128;

static int bits128(u128 q) {
    uint64_t hi = (uint64_t)(q >> 64);
    uint64_t lo = (uint64_t)q;
    if (hi) {
        return 128 - __builtin_clzll(hi);
    } else if (lo) {
        return 64 - __builtin_clzll(lo);
    }
    return 0;
}

// q * 2^e
static long double make(uint64_t q, int e) {
    int s = __builtin_clzll(q);
    uint64_t m = q << s;
    int bias = e + 63 - s + 16383;
    if (bias <= 0) {
        m >>= 1 - bias;
        bias = 0;
    }
    struct {
        uint64_t m;
        uint16_t se;
        uint8_t pad[6];
    } b = {m, (uint16_t)bias, {0}};
    long double v;
    memcpy(&v, &b, sizeof(v));
    return v;
}

static long double round_to_kind(int neg, u128 q, int e, int sticky,
                                 const struct float_kind *k) {
    long double inf = __builtin_infl();
    if (!q) {
        if (sticky) {
            errno = ERANGE;
        }
        return neg ? -0.0L : 0.0L;
    }
    int L = bits128(q);
    int lead = e + L - 1;
    if (lead > k->emax) {
        errno = ERANGE;
        return neg ? -inf : inf;
    }
    int tiny = lead < k->emin;
    int bits = tiny ? k->bits - (k->emin - lead) : k->bits;
    if (bits < 0) {
        errno = ERANGE;
        return neg ? -0.0L : 0.0L;
    }
    int drop = L - bits;
    if (drop > 0) {
        u128 half = (u128)1 << (drop - 1);
        u128 rem = q & (((u128)1 << drop) - 1);
        int inexact = rem || sticky;
        q >>= drop;
        e += drop;
        if (rem > half || (rem == half && (sticky || (q & 1)))) {
            q++;
            if (!tiny && q >> bits) {
                q >>= 1;
                e++;
                if (e + bits - 1 > k->emax) {
                    errno = ERANGE;
                    return neg ? -inf : inf;
                }
            }
        }
        if (tiny && inexact) {
            errno = ERANGE;
        }
        if (!q) {
            errno = ERANGE;
            return neg ? -0.0L : 0.0L;
        }
    } else if (tiny && sticky) {
        errno = ERANGE;
    }
    long double v = make((uint64_t)q, e);
    return neg ? -v : v;
}

/*
 * big integers
 */

#define LIMBS 3000

struct big {
    uint32_t d[LIMBS];
    int n;
};

static void big_mul_add(struct big *b, uint32_t mul, uint32_t add) {
    uint64_t carry = add;
    for (int i = 0; i < b->n; i++) {
        uint64_t t = (uint64_t)b->d[i] * mul + carry;
        b->d[i] = (uint32_t)t;
        carry = t >> 32;
    }
    if (carry) {
        b->d[b->n++] = (uint32_t)carry;
    }
}

static int big_bits(const struct big *b) {
    return b->n ? 32 * (b->n - 1) + 32 - __builtin_clz(b->d[b->n - 1]) : 0;
}

static void big_shl(struct big *b, int s) {
    int w = s / 32;
    int r = s % 32;
    if (!b->n) {
        return;
    }
    b->d[b->n] = 0;
    for (int i = b->n; i >= 0; i--) {
        uint32_t hi = b->d[i] << r;
        uint32_t lo = r && i ? b->d[i - 1] >> (32 - r) : 0;
        if (i + w < LIMBS) {
            b->d[i + w] = hi | lo;
        }
    }
    for (int i = 0; i < w; i++) {
        b->d[i] = 0;
    }
    b->n += w + 1;
    while (b->n && !b->d[b->n - 1]) {
        b->n--;
    }
}

// limb j of b << s
static uint32_t shifted(const struct big *b, int j, int s) {
    int w = s / 32;
    int r = s % 32;
    int i = j - w;
    uint32_t hi = i >= 0 && i < b->n ? b->d[i] << r : 0;
    uint32_t lo = r && i - 1 >= 0 && i - 1 < b->n ? b->d[i - 1] >> (32 - r) : 0;
    return hi | lo;
}

static int big_cmp_shifted(const struct big *a, const struct big *b, int s) {
    int n = a->n > b->n + s / 32 + 1 ? a->n : b->n + s / 32 + 1;
    for (int j = n - 1; j >= 0; j--) {
        uint32_t x = j < a->n ? a->d[j] : 0;
        uint32_t y = shifted(b, j, s);
        if (x != y) {
            return x < y ? -1 : 1;
        }
    }
    return 0;
}

static void big_sub_shifted(struct big *a, const struct big *b, int s) {
    int64_t borrow = 0;
    for (int j = 0; j < a->n; j++) {
        int64_t t = (int64_t)a->d[j] - shifted(b, j, s) - borrow;
        borrow = t < 0;
        a->d[j] = (uint32_t)(t + (borrow ? 1LL << 32 : 0));
    }
    while (a->n && !a->d[a->n - 1]) {
        a->n--;
    }
}

/*
 * decimal
 */

#define MAXDIG 12000

static long double decimal(int neg, const char *dig, int n, long dexp,
                           int sticky, const struct float_kind *k) {
    static struct big N, D;
    if (dexp + n > 4933 + 1) {
        errno = ERANGE;
        return neg ? -__builtin_infl() : __builtin_infl();
    }
    if (dexp + n < -4951 - 1) {
        errno = ERANGE;
        return neg ? -0.0L : 0.0L;
    }
    N.n = 0;
    for (int i = 0; i < n;) {
        uint32_t chunk = 0, mul = 1;
        for (int j = 0; j < 9 && i < n; j++, i++) {
            chunk = chunk * 10 + (uint32_t)(dig[i] - '0');
            mul *= 10;
        }
        big_mul_add(&N, mul, chunk);
    }
    if (!N.n) {
        return neg ? -0.0L : 0.0L;
    }
    if (dexp >= 0) {
        for (long x = dexp; x > 0; x -= 9) {
            uint32_t mul = 1;
            for (long j = 0; j < 9 && j < x; j++) {
                mul *= 10;
            }
            big_mul_add(&N, mul, 0);
        }
        int L = big_bits(&N);
        if (L <= 68) {
            u128 q = 0;
            for (int i = N.n - 1; i >= 0; i--) {
                q = q << 32 | N.d[i];
            }
            return round_to_kind(neg, q, 0, sticky, k);
        }
        int shift = L - 68;
        u128 q = 0;
        for (int b = L - 1; b >= shift; b--) {
            q = q << 1 | ((N.d[b / 32] >> (b % 32)) & 1);
        }
        for (int b = 0; b < shift && !sticky; b++) {
            sticky = (N.d[b / 32] >> (b % 32)) & 1;
        }
        return round_to_kind(neg, q, shift, sticky, k);
    }

    D.n = 1;
    D.d[0] = 1;
    for (long x = -dexp; x > 0; x -= 13) {
        uint32_t mul = 1;
        for (long j = 0; j < 13 && j < x; j++) {
            mul *= 5;
        }
        big_mul_add(&D, mul, 0);
    }
    int s = big_bits(&D) - big_bits(&N) + 68;
    if (s > 0) {
        big_shl(&N, s);
    }
    int ds = s < 0 ? -s : 0;
    u128 q = 0;
    for (int i = 69; i >= 0; i--) {
        if (big_cmp_shifted(&N, &D, i + ds) >= 0) {
            big_sub_shifted(&N, &D, i + ds);
            q |= (u128)1 << i;
        }
    }
    return round_to_kind(neg, q, (int)dexp - s, sticky || N.n, k);
}

/*
 * hexadecimal
 */

static int hexval(int c) {
    return isdigit(c) ? c - '0' : isxdigit(c) ? (c | 0x20) - 'a' + 10 : -1;
}

static long double hex(int neg, const char *p, const char **end,
                       const struct float_kind *k) {
    uint64_t q = 0;
    long e = 0;
    int sticky = 0, any = 0, used = 0, point = 0;
    for (;; p++) {
        if (*p == '.' && !point) {
            point = 1;
            continue;
        }
        int v = hexval((unsigned char)*p);
        if (v < 0) {
            break;
        }
        any = 1;
        if (used < 16) {
            if (q || v) {
                used++;
            }
            q = q << 4 | (uint64_t)v;
            e -= point ? 4 : 0;
        } else {
            sticky |= v != 0;
            e += point ? 0 : 4;
        }
    }
    if (!any) {
        *end = nullptr;
        return 0;
    }
    if ((*p | 0x20) == 'p') {
        const char *x = p + 1;
        int eng = *x == '-';
        if (*x == '-' || *x == '+') {
            x++;
        }
        if (isdigit((unsigned char)*x)) {
            long pe = 0;
            for (; isdigit((unsigned char)*x); x++) {
                pe = pe < 100000 ? pe * 10 + (*x - '0') : pe;
            }
            e += eng ? -pe : pe;
            p = x;
        }
    }
    *end = p;
    if (e > 200000) {
        e = 200000;
    } else if (e < -200000) {
        e = -200000;
    }
    return round_to_kind(neg, q, (int)e, sticky, k);
}

/*
 * entry point
 */

static int match_word(const char *p, const char *w) {
    int i = 0;
    for (; w[i]; i++) {
        if ((p[i] | 0x20) != w[i]) {
            return 0;
        }
    }
    return i;
}

static volatile int lock;

static long double scan(const char *s, char **end, const struct float_kind *k) {
    static char dig[MAXDIG];
    const char *p = s;
    while (isspace((unsigned char)*p)) {
        p++;
    }
    int neg = *p == '-';
    if (*p == '-' || *p == '+') {
        p++;
    }

    if (match_word(p, "inf")) {
        p += match_word(p, "infinity") ? 8 : 3;
        if (end) {
            *end = (char *)p;
        }
        return neg ? -__builtin_infl() : __builtin_infl();
    }

    if (match_word(p, "nan")) {
        p += 3;
        if (*p == '(') {
            const char *q = p + 1;
            while (isalnum((unsigned char)*q) || *q == '_') {
                q++;
            }
            if (*q == ')') {
                p = q + 1;
            }
        }
        if (end) {
            *end = (char *)p;
        }
        long double nan = __builtin_nanf("");
        return neg ? -nan : nan;
    }

    // hexadecimal
    if (p[0] == '0' && (p[1] | 0x20) == 'x') {
        const char *e;
        long double v = hex(neg, p + 2, &e, k);
        if (e) {
            if (end) {
                *end = (char *)e;
            }
            return v;
        }
    }

    // decimal
    long dexp = 0;
    int nd = 0, sticky = 0, any = 0, point = 0;
    for (;; p++) {
        if (*p == '.' && !point) {
            point = 1;
            continue;
        }
        if (!isdigit((unsigned char)*p)) {
            break;
        }
        any = 1;
        if (*p == '0' && !nd) {
            dexp -= point;
            continue;
        }
        if (nd < MAXDIG) {
            dig[nd++] = *p;
            dexp -= point;
        } else {
            sticky |= *p != '0';
            dexp += !point;
        }
    }
    if (!any) {
        if (end) {
            *end = (char *)s;
        }
        return 0;
    }
    if ((*p | 0x20) == 'e') {
        const char *x = p + 1;
        int eng = *x == '-';
        if (*x == '-' || *x == '+') {
            x++;
        }
        if (isdigit((unsigned char)*x)) {
            long de = 0;
            for (; isdigit((unsigned char)*x); x++) {
                de = de < 100000 ? de * 10 + (*x - '0') : de;
            }
            dexp += eng ? -de : de;
            p = x;
        }
    }
    if (end) {
        *end = (char *)p;
    }
    return decimal(neg, dig, nd, dexp, sticky, k);
}

long double __floatscan(const char *s, char **end, const struct float_kind *k) {
    __lock(&lock);
    long double v = scan(s, end, k);
    __unlock(&lock);
    return v;
}
