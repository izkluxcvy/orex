#include <limits.h>

#include <pthread_impl.h>

struct __libc_threads __libc_threads;

int __timedwait_p(volatile int *addr, int val, clockid_t clock,
                  const struct timespec *abs, int cp, int shared) {
    if (abs && (abs->tv_nsec < 0 || abs->tv_nsec >= 1'000'000'000L)) {
        return EINVAL;
    }
    if (clock != CLOCK_REALTIME && clock != CLOCK_MONOTONIC) {
        return EINVAL;
    }
    long op = (shared ? 9 : FUTEX_WAIT_BITSET_PRIVATE) |
              (clock == CLOCK_REALTIME ? FUTEX_CLOCK_REALTIME : 0);
    long r = cp ? __syscall_cp(SYS_futex, (long)addr, op, val, (long)abs, 0, -1)
                : __syscall6(SYS_futex, (long)addr, op, val, (long)abs, 0, -1);
    switch (r) {
    case -ETIMEDOUT:
        return ETIMEDOUT;
    case -EINTR:
        return EINTR;
    case -EINVAL:
        return EINVAL;
    default:
        return 0;
    }
}

void __lock(volatile int *l) {
    if (!__libc_threads.threaded) {
        *l = 1;
        return;
    }
    if (__sync_val_compare_and_swap(l, 0, 1) == 0) {
        return;
    }
    while (__atomic_exchange_n(l, 2, __ATOMIC_ACQUIRE) != 0) {
        __timedwait(l, 2, CLOCK_MONOTONIC, nullptr, 0);
    }
}

void __unlock(volatile int *l) {
    if (__atomic_exchange_n(l, 0, __ATOMIC_RELEASE) == 2) {
        __wake(l, 1);
    }
}

void __lock_p(volatile int *l, int shared) {
    if (__sync_val_compare_and_swap(l, 0, 1) == 0) {
        return;
    }
    while (__atomic_exchange_n(l, 2, __ATOMIC_ACQUIRE) != 0) {
        __timedwait_p(l, 2, CLOCK_MONOTONIC, nullptr, 0, shared);
    }
}

void __unlock_p(volatile int *l, int shared) {
    if (__atomic_exchange_n(l, 0, __ATOMIC_RELEASE) == 2) {
        __wake_p(l, 1, shared);
    }
}
