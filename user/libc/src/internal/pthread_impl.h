#pragma once

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <syscall.h>
#include <tcb.h>

#define DT_JOINABLE 0
#define DT_DETACHED 1
#define DT_EXITING  2

#define SIGCANCEL 32
#define SIGTIMER  33

struct robust_head {
    void *list;
    long offset;
    void *pending;
};

struct pthread {
    struct tcb tcb;

    volatile int tid;
    volatile int detach;
    volatile int cancel;
    int cancel_disable;
    int cancel_async;
    void *(*start)(void *);
    void *arg;
    void *result;
    unsigned long start_mask;
    void *map_base;
    size_t map_size;
    struct __ptcb *cleanup;
    struct robust_head robust;
    void *tsb[PTHREAD_KEYS_MAX];
    unsigned tsd_gen[PTHREAD_KEYS_MAX];
    int tsd_used;
    struct __locale_struct *locale;
    void **dtv;
    size_t dtv_n;
    volatile int sched_hold;
};

static inline struct pthread *__pthread_self() {
    struct pthread *self;
    __asm__("mov %0, fs:0" : "=r"(self));
    return self;
}

struct __libc_threads {
    int threaded;
    volatile int threads_minus_1;
};
extern struct __libc_threads __libc_threads;

#define TLS_MODULES_MAX 256
struct tls_module {
    const void *image;
    size_t filesz, size, align, offset;
};
#define __TLS_HIDDEN __attribute__((visibility("hidden")))
extern struct tls_module __tls_mods[TLS_MODULES_MAX] __TLS_HIDDEN;
extern size_t __tls_nmods __TLS_HIDDEN, __tls_nstatic __TLS_HIDDEN,
    __tls_size __TLS_HIDDEN, __tls_align __TLS_HIDDEN;
__TLS_HIDDEN size_t __tls_add_static(const void *image, size_t filesz,
                                     size_t memsz, size_t align);
__TLS_HIDDEN size_t __tls_add_dynamic(const void *image, size_t filesz,
                                      size_t memsz, size_t align);
void *__tls_get_addr(void *ti);
__attribute__((weak)) void __tls_free_dynamic(struct pthread *self);

__TLS_HIDDEN struct pthread *__tls_place(void *mem, size_t size);
__TLS_HIDDEN size_t __tls_area_size();

#define FUTEX_WAIT_BITSET_PRIVATE (9 | 128)
#define FUTEX_WAKE_PRIVATE        (1 | 128)
#define FUTEX_CLOCK_REALTIME      256

static inline void __wake_p(volatile void *addr, int n, int shared) {
    __syscall3(SYS_futex, (long)addr, shared ? 1 : FUTEX_WAKE_PRIVATE, n);
}

static inline void __wake(volatile void *addr, int n) { __wake_p(addr, n, 0); }

int __timedwait_p(volatile int *addr, int val, clockid_t clock,
                  const struct timespec *abs, int cp, int shared);

static inline int __timedwait(volatile int *addr, int val, clockid_t clock,
                              const struct timespec *abs, int cp) {
    return __timedwait_p(addr, val, clock, abs, cp, 0);
}

long __syscall_cp(long n, long a, long b, long c, long d, long e, long f);
#define __scp1(n, a)    __syscall_cp(n, (long)(a), 0, 0, 0, 0, 0)
#define __scp2(n, a, b) __syscall_cp(n, (long)(a), (long)(b), 0, 0, 0, 0)
#define __scp3(n, a, b, c)                                                     \
    __syscall_cp(n, (long)(a), (long)(b), (long)(c), 0, 0, 0)
#define __scp4(n, a, b, c, d)                                                  \
    __syscall_cp(n, (long)(a), (long)(b), (long)(c), (long)(d), 0, 0)
#define __scp5(n, a, b, c, d, e)                                               \
    __syscall_cp(n, (long)(a), (long)(b), (long)(c), (long)(d), (long)(e), 0)
#define __scp6(n, a, b, c, d, e, f)                                            \
    __syscall_cp(n, (long)(a), (long)(b), (long)(c), (long)(d), (long)(e),     \
                 (long)(f))
#define __syscall_cp1(n, a) __syscall_cp(n, (long)(a), 0, 0, 0, 0, 0)
#define __syscall_cp3(n, a, b, c)                                              \
    __syscall_cp(n, (long)(a), (long)(b), (long)(c), 0, 0, 0)
#define __syscall_cp4(n, a, b, c, d)                                           \
    __syscall_cp(n, (long)(a), (long)(b), (long)(c), (long)(d), 0, 0)

void __lock(volatile int *l);
void __unlock(volatile int *l);
void __lock_p(volatile int *l, int shared);
void __unlock_p(volatile int *l, int shared);

#define FUTEX_WAITERS    0x80000000u
#define FUTEX_OWNER_DIED 0x40000000u
#define FUTEX_TID_MASK   0x3fffffffu

void __pthread_robust_init(struct pthread *self);

void __pthread_tsd_run_dtors(struct pthread *self);
void __pthread_robust_release(struct pthread *self);
[[noreturn]] void __pthread_exit_cancel(void);
void __fork_prepare(void);
void __fork_done(int child);
