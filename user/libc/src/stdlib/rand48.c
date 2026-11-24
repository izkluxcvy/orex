#include <stdint.h>
#include <stdlib.h>

// X' = (a * X + c) mod 2^48

static unsigned short x[3] = {0x330e, 0xabcd, 0x1234};
static uint64_t a = 0x5deece66dULL;
static uint16_t c = 0xb;

static uint64_t get(const unsigned short s[3]) {
    return s[0] | (uint64_t)s[1] << 16 | (uint64_t)s[2] << 32;
}

static uint64_t step(unsigned short s[3]) {
    uint64_t v = (a * get(s) + c) & ((1ULL << 48) - 1);
    s[0] = (unsigned short)v;
    s[1] = (unsigned short)(v >> 16);
    s[2] = (unsigned short)(v >> 32);
    return v;
}

double erand48(unsigned short s[3]) { return (double)step(s) * 0x1p-48; }
long nrand48(unsigned short s[3]) { return (long)(step(s) >> 17); }
long jrand48(unsigned short s[3]) { return (long)step(s) >> 16; }
double drand48() { return erand48(x); }
long lrand48() { return nrand48(x); }
long mrand48() { return jrand48(x); }

void srand48(long seed) {
    x[0] = 0x330e;
    x[1] = (unsigned short)seed;
    x[2] = (unsigned short)(seed >> 16);
    a = 0x5deece66dULL;
    c = 0xb;
}

unsigned short *seed48(unsigned short s[3]) {
    static unsigned short old[3];
    old[0] = x[0];
    old[1] = x[1];
    old[2] = x[2];
    x[0] = s[0];
    x[1] = s[1];
    x[2] = s[2];
    a = 0x5deece66dULL;
    c = 0xb;
    return old;
}

void lcong48(unsigned short p[7]) {
    x[0] = p[0];
    x[1] = p[1];
    x[2] = p[2];
    a = get(p + 3);
    c = p[6];
}
