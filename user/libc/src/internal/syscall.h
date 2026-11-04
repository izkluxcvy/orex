#pragma once

#include <sys/syscall.h>

static inline long __syscall6(long n, long a, long b, long c, long d, long e,
                              long f) {
    long ret;
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    __asm__ __volatile__("syscall"
                         : "=a"(ret)
                         : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8),
                           "r"(r9)
                         : "rcx", "r11", "memory");
    return ret;
}

static inline long __syscall0(long n) {
    return __syscall6(n, 0, 0, 0, 0, 0, 0);
}
static inline long __syscall1(long n, long a) {
    return __syscall6(n, a, 0, 0, 0, 0, 0);
}
static inline long __syscall2(long n, long a, long b) {
    return __syscall6(n, a, b, 0, 0, 0, 0);
}
static inline long __syscall3(long n, long a, long b, long c) {
    return __syscall6(n, a, b, c, 0, 0, 0);
}
static inline long __syscall4(long n, long a, long b, long c, long d) {
    return __syscall6(n, a, b, c, d, 0, 0);
}
static inline long __syscall5(long n, long a, long b, long c, long d, long e) {
    return __syscall6(n, a, b, c, d, e, 0);
}

long __syscall_ret(long r);
