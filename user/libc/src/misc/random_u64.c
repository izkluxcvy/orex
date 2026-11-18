#include <stdint.h>
#include <sys/auxv.h>
#include <unistd.h>

#include <libc.h>

uint64_t __random_u64() {
    static uint64_t state;
    if (!state) {
        const uint64_t *r = (const uint64_t *)__getauxval(AT_RANDOM);
        state = ((r ? r[1] : 0) ^ ((uint64_t)getpid() << 32)) | 1;
    }
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    uint64_t z = (state += 0x9e3779b97f4a7c15ULL ^ ((uint64_t)hi << 32 | lo));
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
