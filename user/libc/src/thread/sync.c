#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>

#include <pthread_impl.h>

/*
 * read-write locks
 */

static int pshared_ok(int pshared) {
    return pshared == PTHREAD_PROCESS_SHARED ||
                   pshared == PTHREAD_PROCESS_PRIVATE
               ? 0
               : EINVAL;
}

int pthread_rwlockattr_init(pthread_rwlockattr_t *a) {
    a->__attr = 0;
    return 0;
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t *a) {
    (void)a;
    return 0;
}

int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *__restrict a,
                                  int *__restrict pshared) {
    *pshared = (int)a->__attr;
    return 0;
}

int pshared_rwlockattr_setpshared(pthread_rwlockattr_t *a, int pshared) {
    int e = pshared_ok(pshared);
    if (!e) {
        a->__attr = (unsigned)pshared;
    }
    return e;
}

int pthread_rwlock_init(pthread_rwlock_t *__restrict l,
                        const pthread_rwlockattr_t *__restrict a) {
    *l = (pthread_rwlock_t){.__shared = a && a->__attr};
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *l) {
    return l->__readers || l->__writer ? EBUSY : 0;
}

static int rwlock(pthread_rwlock_t *l, int write, int try, clockid_t clock,
                  const struct timespec *abs) {
    int tid = __pthread_self()->tid;
    while (1) {
        __lock_p(&l->__lock, l->__shared);
        if (l->__writer == tid) {
            __unlock_p(&l->__lock, l->__shared);
            return EDEADLK;
        }
        int free = write ? !l->__writer && !l->__readers : !l->__writer;
        if (free) {
            if (write) {
                l->__writer = tid;
            } else if (l->__readers == INT32_MAX) {
                __unlock_p(&l->__lock, l->__shared);
                return EAGAIN;
            } else {
                l->__readers++;
            }
            __unlock_p(&l->__lock, l->__shared);
            return 0;
        }
        if (try) {
            __unlock_p(&l->__lock, l->__shared);
            return EBUSY;
        }
        int seq = l->__seq;
        l->__waiters++;
        __unlock_p(&l->__lock, l->__shared);
        int e = __timedwait_p(&l->__seq, seq, clock, abs, 0, l->__shared);
        __lock_p(&l->__lock, l->__shared);
        l->__waiters--;
        __unlock_p(&l->__lock, l->__shared);
        if (e == ETIMEDOUT || e == EINVAL) {
            return e;
        }
    }
}

int pthread_rwlock_rdlock(pthread_rwlock_t *l) {
    return rwlock(l, 0, 0, CLOCK_REALTIME, nullptr);
}

int pthread_rwlock_wrlock(pthread_rwlock_t *l) {
    return rwlock(l, 1, 0, CLOCK_REALTIME, nullptr);
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *l) {
    return rwlock(l, 0, 1, CLOCK_REALTIME, nullptr);
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *l) {
    return rwlock(l, 1, 1, CLOCK_REALTIME, nullptr);
}

int pthread_rwlock_timedrdlock(pthread_rwlock_t *__restrict l,
                               const struct timespec *__restrict abs) {
    return rwlock(l, 0, 0, CLOCK_REALTIME, abs);
}

int pthread_rwlock_timedwrlock(pthread_rwlock_t *__restrict l,
                               const struct timespec *__restrict abs) {
    return rwlock(l, 1, 0, CLOCK_REALTIME, abs);
}

int pthread_rwlock_unlock(pthread_rwlock_t *l) {
    int tid = __pthread_self()->tid;
    int err = 0;
    __lock_p(&l->__lock, l->__shared);
    if (l->__writer == tid) {
        l->__writer = 0;
    } else if (l->__readers) {
        l->__readers--;
    } else {
        err = EPERM;
    }
    int wake = !err && l->__waiters;
    if (wake) {
        l->__seq++;
    }
    __unlock_p(&l->__lock, l->__shared);
    if (wake) {
        __wake_p(&l->__seq, INT_MAX, l->__shared);
    }
    return err;
}

/*
 * barriers
 */

int pthread_barrierattr_init(pthread_barrierattr_t *a) {
    a->__attr = 0;
    return 0;
}

int pthread_barrierattr_destroy(pthread_barrierattr_t *a) {
    (void)a;
    return 0;
}

int pthread_barrierattr_getpshared(const pthread_barrierattr_t *__restrict a,
                                   int *__restrict pshared) {
    *pshared = (int)a->__attr;
    return 0;
}

int pthread_barrierattr_setpshared(pthread_barrierattr_t *a, int pshared) {
    int e = pshared_ok(pshared);
    if (!e) {
        a->__attr = (unsigned)pshared;
    }
    return e;
}

int pthread_barrier_init(pthread_barrier_t *__restrict b,
                         const pthread_barrierattr_t *__restrict a,
                         unsigned count) {
    if (!count) {
        return EINVAL;
    }
    *b = (pthread_barrier_t){.__count = count, .__shared = a && a->__attr};
    return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *b) {
    return b->__arrived ? EBUSY : 0;
}

int pthread_barrier_wait(pthread_barrier_t *b) {
    __lock_p(&b->__lock, b->__shared);
    int seq = b->__seq;
    if (++b->__arrived == b->__count) {
        b->__arrived = 0;
        b->__seq++;
        __unlock_p(&b->__lock, b->__shared);
        __wake_p(&b->__seq, INT_MAX, b->__shared);
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }
    __unlock_p(&b->__lock, b->__shared);
    while (b->__seq == seq) {
        __timedwait_p(&b->__seq, seq, CLOCK_MONOTONIC, nullptr, 0, b->__shared);
    }
    return 0;
}

/*
 * spinlocks
 */

int pthread_spin_init(pthread_spinlock_t *l, int pshared) {
    int e = pshared_ok(pshared);
    if (!e) {
        *l = 0;
    }
    return e;
}

int pthread_spin_destroy(pthread_spinlock_t *l) {
    (void)l;
    return 0;
}

int pthread_spinlock(pthread_spinlock_t *l) {
    int tid = __pthread_self()->tid;
    if (*l == tid) {
        return EDEADLK;
    }
    for (int n = 0; __sync_val_compare_and_swap(l, 0, tid) != 0; n++) {
        if (n & 15) {
            __builtin_ia32_pause();
        } else {
            sched_yield();
        }
    }
    return 0;
}

int pthread_spin_trylock(pthread_spinlock_t *l) {
    return __sync_val_compare_and_swap(l, 0, __pthread_self()->tid) ? EBUSY : 0;
}

int pthread_spin_unlock(pthread_spinlock_t *l) {
    __atomic_store_n(l, 0, __ATOMIC_RELEASE);
    return 0;
}

/*
 * once
 */

#define ONCE_RUNNING 1
#define ONCE_WAITED  2
#define ONCE_DONE    3

static void once_undo(void *p) {
    volatile int *o = p;
    if (__atomic_exchange_n(o, 0, __ATOMIC_SEQ_CST) == ONCE_WAITED) {
        __wake(o, INT_MAX);
    }
}

int pthread_once(pthread_once_t *o, void (*init)()) {
    volatile int *v = (volatile int *)o;
    while (1) {
        int s = *v;
        if (s == ONCE_DONE) {
            return 0;
        }
        if (s == 0) {
            if (__sync_val_compare_and_swap(v, 0, ONCE_RUNNING) != 0) {
                continue;
            }
            pthread_cleanup_push(once_undo, (void *)v);
            init();
            pthread_cleanup_pop(0);
            if (__atomic_exchange_n(v, ONCE_DONE, __ATOMIC_SEQ_CST) ==
                ONCE_WAITED) {
                __wake(v, INT_MAX);
            }
            return 0;
        }
        if (s == ONCE_RUNNING &&
            __sync_val_compare_and_swap(v, ONCE_RUNNING, ONCE_WAITED) !=
                ONCE_RUNNING) {
            continue;
        }
        __timedwait(v, ONCE_WAITED, CLOCK_MONOTONIC, nullptr, 0);
    }
}

/*
 * thread-specific data
 */

static void (*dtors[PTHREAD_KEYS_MAX])(void *);
static unsigned gens[PTHREAD_KEYS_MAX]; // odd while in use
static volatile int key_lock;

static void no_dtor(void *p) { (void)p; }

int pthread_key_create(pthread_key_t *key, void (*dtor)(void *)) {
    __lock(&key_lock);
    for (unsigned k = 0; k < PTHREAD_KEYS_MAX; k++) {
        if (!(gens[k] & 1)) {
            gens[k]++;
            dtors[k] = dtor ? dtor : no_dtor;
            __unlock(&key_lock);
            *key = k;
            return 0;
        }
    }
    __unlock(&key_lock);
    return EAGAIN;
}

int pthread_key_delete(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX || !(gens[key] & 1)) {
        return EINVAL;
    }
    __lock(&key_lock);
    gens[key]++;
    dtors[key] = nullptr;
    __unlock(&key_lock);
    return 0;
}

void *pthread_getspecific(pthread_key_t key) {
    struct pthread *self = __pthread_self();
    return key < PTHREAD_KEYS_MAX && self->tsd_gen[key] == gens[key]
               ? self->tsd[key]
               : nullptr;
}

int pthread_setspecific(pthread_key_t key, const void *value) {
    if (key >= PTHREAD_KEYS_MAX || !(gens[key] & 1)) {
        return EINVAL;
    }
    struct pthread *self = __pthread_self();
    self->tsd[key] = (void *)value;
    self->tsd_gen[key] = gens[key];
    self->tsd_used = 1;
    return 0;
}

void __pthread_tsd_run_dtors(struct pthread *self) {
    for (int round = 0; self->tsd_used && round < PTHREAD_DESTRUCTOR_ITERATIONS;
         round++) {
        self->tsd_used = 0;
        for (unsigned k = 0; k < PTHREAD_KEYS_MAX; k++) {
            void *v = pthread_getspecific(k);
            void (*d)(void *) = dtors[k];
            if (v && d) {
                self->tsd[k] = nullptr;
                d(v);
            }
        }
    }
}
