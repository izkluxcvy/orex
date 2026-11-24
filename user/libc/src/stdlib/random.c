#include <stdint.h>
#include <stdlib.h>

struct kind {
    int deg, sep;
};

static const struct kind kinds[] = {{0, 0}, {7, 3}, {15, 1}, {31, 3}, {63, 1}};

#define NKINDS 5

static int32_t default_buf[32];
static int32_t *state = default_buf + 1;
static int type = 3, rear, front;
static int seeded;

static int32_t lcg(int32_t word) {
    int32_t hi = word / 127773;
    int32_t lo = word % 127773;
    word = 16807 * lo - 2836 * hi;
    return word < 0 ? word + 2147483647 : word;
}

static void save() { state[-1] = NKINDS * rear + type; }

static long next() {
    if (type == 0) {
        state[0] =
            (int32_t)(((uint32_t)state[0] * 1103515245U + 12345U) & 0x7fffffff);
        return state[0];
    }
    int deg = kinds[type].deg;
    uint32_t v = (uint32_t)state[front] + (uint32_t)state[rear];
    state[front] = (int32_t)v;
    front = (front + 1) % deg;
    rear = (rear + 1) % deg;
    save();
    return (long)(v >> 1);
}

void srandom(unsigned seed) {
    seeded = 1;
    state[0] = (int32_t)(seed ? seed : 1);
    if (type == 0) {
        return;
    }
    int deg = kinds[type].deg;
    for (int i = 1; i < deg; i++) {
        state[i] = lcg(state[i - 1]);
    }
    front = kinds[type].sep;
    rear = 0;
    for (int i = 0; i < 10 * deg; i++) {
        next();
    }
    save();
}

long random() {
    if (!seeded) {
        srandom(1);
    }
    return next();
}

char *initstate(unsigned seed, char *buf, size_t size) {
    if (size < 8) {
        return nullptr;
    }
    char *old = (char *)(state - 1);
    save();
    int t = size < 32 ? 0 : size < 64 ? 1 : size < 128 ? 2 : size < 256 ? 3 : 4;
    type = t;
    state = (int32_t *)buf + 1;
    srandom(seed);
    return old;
}

char *setstate(char *buf) {
    int32_t *s = (int32_t *)buf;
    int t = s[0] % NKINDS;
    int r = s[0] / NKINDS;
    if (t < 0 || (t && r >= kinds[t].deg)) {
        return nullptr;
    }
    char *old = (char *)(state - 1);
    save();
    state = s + 1;
    type = t;
    rear = r;
    front = t ? (r + kinds[t].sep) % kinds[t].deg : 0;
    seeded = 1;
    return old;
}

int rand() { return (int)random(); }
void srand(unsigned seed) { srandom(seed); }

int rand_r(unsigned *seed) {
    unsigned n = *seed;
    int r;
    n = n * 1103515245 + 12345;
    r = (int)((n / 65536) % 2048);
    n = n * 1103515245 + 12345;
    r = (r << 10) ^ (int)((n / 65536) % 1024);
    n = n * 1103515245 + 12345;
    r = (r << 10) ^ (int)((n / 65536) % 1024);
    *seed = n;
    return r;
}
