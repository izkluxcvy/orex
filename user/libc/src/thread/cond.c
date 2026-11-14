#include <errno.h>
#include <limits.h>
#include <pthread.h>

#include <pthread_impl.h>

int __mutex_release_all(pthread_mutex_t *m, int *count);
int __mutex_reacquire(pthread_mutex_t *m, int count);

#define CA_SHARED 0x8000'0000U

int pthread_condattr_init(pthread_condattr_t *a) {
    a->__attr = CLOCK_REALTIME;
    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *a) {
    (void)a;
    return 0;
}

int pthread_condattr_getclock(const pthread_condattr_t *__restrict a,
                              clockid_t *__restrict clock) {
    *clock = (clockid_t)(a->__attr & ~CA_SHARED);
    return 0;
}

int pthread_condattr_setclock(pthread_condattr_t *a, clockid_t clock) {
    if (clock != CLOCK_REALTIME && clock != CLOCK_MONOTONIC) {
        return EINVAL;
    }
    a->__attr = (a->__attr & CA_SHARED) | (unsigned)clock;
    return 0;
}

int pthread_condattr_getpshared(const pthread_condattr_t *__restrict a,
                                int *__restrict pshared) {
    *pshared = (a->__attr & CA_SHARED) != 0;
    return 0;
}

int pthread_condattr_setpshared(pthread_condattr_t *a, int pshared) {
    if (pshared != PTHREAD_PROCESS_PRIVATE &&
        pshared != PTHREAD_PROCESS_SHARED) {
        return EINVAL;
    }
    a->__attr = pshared ? a->__attr | CA_SHARED : a->__attr & ~CA_SHARED;
    return 0;
}

int pthread_cond_init(pthread_cond_t *__restrict c,
                      const pthread_condattr_t *__restrict a) {
    *c = (pthread_cond_t){0};
    c->__clock = a ? (int)(a->__attr & ~CA_SHARED) : CLOCK_REALTIME;
    c->__shared = a && (a->__attr & CA_SHARED);
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *c) { return c->__waiters ? EBUSY : 0; }

struct waiting {
    pthread_cond_t *c;
    pthread_mutex_t *m;
    int count;
};

static void unwait(void *p) {
    struct waiting *w = p;
    __atomic_fetch_sub(&w->c->__waiters, 1, __ATOMIC_SEQ_CST);
    __mutex_reacquire(w->m, w->count);
}

static int wait(pthread_cond_t *__restrict c, pthread_mutex_t *__restrict m,
                clockid_t clock, const struct timespec *__restrict abs) {
    if (abs && (abs->tv_nsec < 0 || abs->tv_nsec >= 1'000'000'000L)) {
        return EINVAL;
    }
    struct waiting w = {c, m, 0};
    int seq = c->__seq;
    __atomic_fetch_add(&c->__waiters, 1, __ATOMIC_SEQ_CST);
    int r = __mutex_release_all(m, &w.count);
    if (r) {
        __atomic_fetch_sub(&c->__waiters, 1, __ATOMIC_SEQ_CST);
        return r;
    }
    int e;
    pthread_cleanup_push(unwait, &w);
    do {
        e = __timedwait_p(&c->__seq, seq, clock, abs, 1, c->__shared);
    } while (e == EINTR && c->__seq == seq);
    pthread_cleanup_pop(0);
    __atomic_fetch_sub(&c->__waiters, 1, __ATOMIC_SEQ_CST);
    r = __mutex_reacquire(m, w.count);
    return r ? r : e == ETIMEDOUT ? ETIMEDOUT : 0;
}

int pthread_cond_wait(pthread_cond_t *__restrict c,
                      pthread_mutex_t *__restrict m) {
    return wait(c, m, c->__clock, nullptr);
}

int pthread_cond_timedwait(pthread_cond_t *__restrict c,
                           pthread_mutex_t *__restrict m,
                           const struct timespec *__restrict abs) {
    return wait(c, m, c->__clock, abs);
}

int pthread_cond_clockwait(pthread_cond_t *__restrict c,
                           pthread_mutex_t *__restrict m, clockid_t clock,
                           const struct timespec *__restrict abs) {
    return wait(c, m, clock, abs);
}

int pthread_cond_signal(pthread_cond_t *c) {
    if (c->__waiters) {
        __atomic_fetch_add(&c->__seq, 1, __ATOMIC_SEQ_CST);
        __wake_p(&c->__seq, 1, c->__shared);
    }
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *c) {
    if (c->__waiters) {
        __atomic_fetch_add(&c->__seq, 1, __ATOMIC_SEQ_CST);
        __wake_p(&c->__seq, INT_MAX, c->__shared);
    }
    return 0;
}
