#include <errno.h>
#include <pthread.h>

#include <pthread_impl.h>

#define KIND           3
#define ROBUST         4
#define INCONSISTENT   8
#define NOTRECOVERABLE 16
#define PSHARED        32

#define A_KIND(a)  ((a) & 3)
#define A_ROBUST   4
#define A_PSHARED  8
#define A_PROTO(a) (((a) >> 4) & 3)

int pthread_mutexattr_init(pthread_mutexattr_t *a) {
    a->__attr = 0;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *a) {
    (void)a;
    return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t *__restrict a,
                              int *__restrict type) {
    *type = (int)A_KIND(a->__attr);
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *a, int type) {
    if (type < 0 || type > PTHREAD_MUTEX_ERRORCHECK) {
        return EINVAL;
    }
    a->__attr = (a->__attr & ~3U) | (unsigned)type;
    return 0;
}

int pthread_mutexattr_getrobust(const pthread_mutexattr_t *__restrict a,
                                int *__restrict robust) {
    *robust = (a->__attr & A_ROBUST) != 0;
    return 0;
}

int pthread_mutexattr_setrobust(pthread_mutexattr_t *a, int robust) {
    if (robust != PTHREAD_MUTEX_STALLED && robust != PTHREAD_MUTEX_ROBUST) {
        return EINVAL;
    }
    a->__attr = robust ? a->__attr | A_ROBUST : a->__attr & ~A_ROBUST;
    return 0;
}

int pthread_mutexattr_getpshared(const pthread_mutexattr_t *__restrict a,
                                 int *__restrict pshared) {
    *pshared = (a->__attr & A_PSHARED) != 0;
    return 0;
}

int pthread_mutexattr_setpshared(pthread_mutexattr_t *a, int pshared) {
    if (pshared != PTHREAD_PROCESS_PRIVATE &&
        pshared != PTHREAD_PROCESS_SHARED) {
        return EINVAL;
    }
    a->__attr = pshared ? a->__attr | A_PSHARED : a->__attr & ~A_PSHARED;
    return 0;
}

int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *__restrict a,
                                  int *__restrict protocol) {
    *protocol = (int)A_PROTO(a->__attr);
    return 0;
}

int pthread_mutexattr_setprotocol(pthread_mutexattr_t *a, int protocol) {
    (void)a;
    if (protocol == PTHREAD_PRIO_INHERIT || protocol == PTHREAD_PRIO_PROTECT) {
        return ENOTSUP;
    }
    return protocol == PTHREAD_PRIO_NONE ? 0 : EINVAL;
}

int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *__restrict a,
                                     int *__restrict ceiling) {
    (void)a, (void)ceiling;
    return EINVAL;
}

int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *a, int ceiling) {
    (void)a, (void)ceiling;
    return EINVAL;
}

int pthread_mutex_getprioceiling(const pthread_mutex_t *__restrict m,
                                 int *__restrict ceiling) {
    (void)m, (void)ceiling;
    return EINVAL;
}

int pthread_mutex_setprioceiling(pthread_mutex_t *__restrict m, int ceiling,
                                 int *__restrict old) {
    (void)m, (void)ceiling, (void)old;
    return EINVAL;
}

int pthread_mutex_init(pthread_mutex_t *__restrict m,
                       const pthread_mutexattr_t *__restrict a) {
    *m = (pthread_mutex_t){0};
    if (a) {
        m->__type =
            (int)(A_KIND(a->__attr) | (a->__attr & A_ROBUST ? ROBUST : 0) |
                  (a->__attr & A_PSHARED ? PSHARED : 0));
    }
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *m) { return m->__lock ? EBUSY : 0; }

/*
 * robust list
 */

#define ENTRY(m) ((void **)&(m)->__next)
#define MUTEX(e)                                                               \
    ((pthread_mutex_t *)((char *)(e) - offsetof(pthread_mutex_t, __next)))

void __pthread_robust_init(struct pthread *self) {
    self->robust.list = &self->robust.list;
    self->robust.offset = (long)offsetof(pthread_mutex_t, __lock) -
                          (long)offsetof(pthread_mutex_t, __next);
    self->robust.pending = nullptr;
    __syscall2(SYS_get_robust_list, (long)&self->robust, 24);
}

static void robust_link(struct pthread *self, pthread_mutex_t *m) {
    void **head = &self->robust.list;
    m->__next = *head;
    m->__prev = head;
    if (*head != head) {
        MUTEX(*head)->__prev = ENTRY(m);
    }
    *head = ENTRY(m);
}

static void robust_unlink(struct pthread *self, pthread_mutex_t *m) {
    void **prev = m->__prev;
    void **next = m->__next;
    if (next != &self->robust.list) {
        MUTEX(next)->__prev = prev;
    }
}

static int shared(pthread_mutex_t *m) { return (m->__type & PSHARED) != 0; }

static int relock(pthread_mutex_t *m, int tid) {
    int type = m->__type;
    if (m->__owner != tid ||
        ((type & KIND) == PTHREAD_MUTEX_NORMAL && !(type & ROBUST))) {
        return -1;
    }
    if ((type & KIND) == PTHREAD_MUTEX_RECURSIVE) {
        if (m->__count == INT32_MAX) {
            return EAGAIN;
        }
        m->__count++;
        return 0;
    }
    return EDEADLK;
}

static int robust_lock(pthread_mutex_t *m, struct pthread *self, int try,
                       clockid_t clock, const struct timespec *abs) {
    int tid = self->tid;
    while (1) {
        if (m->__type & NOTRECOVERABLE) {
            return ENOTRECOVERABLE;
        }
        unsigned w = (unsigned)m->__lock;
        if (!(w & FUTEX_TID_MASK)) {
            self->robust.pending = ENTRY(m);
            unsigned nw = (unsigned)tid | (w & FUTEX_WAITERS);
            if (__sync_bool_compare_and_swap(&m->__lock, (int)w, (int)nw)) {
                robust_link(self, m);
                self->robust.pending = nullptr;
                m->__owner = tid;
                m->__count = 0;
                if (w & FUTEX_OWNER_DIED) {
                    m->__type |= INCONSISTENT;
                    return EOWNERDEAD;
                }
                return 0;
            }
            self->robust.pending = nullptr;
            continue;
        }
        if (try) {
            return EBUSY;
        }
        unsigned ww = w | FUTEX_WAITERS;
        if (w != ww &&
            !__sync_bool_compare_and_swap(&m->__lock, (int)w, (int)ww)) {
            continue;
        }
        int e = __timedwait_p(&m->__lock, (int)ww, clock, abs, 0, shared(m));
        if (e == ETIMEDOUT || e == EINVAL) {
            return e;
        }
    }
}

static int lock(pthread_mutex_t *m, clockid_t clock,
                const struct timespec *abs) {
    struct pthread *self = __pthread_self();
    int r = relock(m, self->tid);
    if (r >= 0) {
        return r;
    }
    if (m->__type & ROBUST) {
        return robust_lock(m, self, 0, clock, abs);
    }
    if (__sync_val_compare_and_swap(&m->__lock, 0, 1) != 0) {
        for (int spin = 0; spin < 100 && m->__lock; spin++) {
            __builtin_ia32_pause();
        }
        while (__atomic_exchange_n(&m->__lock, 2, __ATOMIC_ACQUIRE) != 0) {
            int e = __timedwait_p(&m->__lock, 2, clock, abs, 0, shared(m));
            if (e == ETIMEDOUT || e == EINVAL) {
                return e;
            }
        }
    }
    m->__owner = self->tid;
    m->__count = 0;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *m) {
    return lock(m, CLOCK_REALTIME, nullptr);
}

int pthread_mutex_timedlock(pthread_mutex_t *__restrict m,
                            const struct timespec *__restrict abs) {
    return lock(m, CLOCK_REALTIME, abs);
}

int pthread_mutex_clocklock(pthread_mutex_t *__restrict m, clockid_t clock,
                            const struct timespec *__restrict abs) {
    return lock(m, clock, abs);
}

int pthread_mutex_trylock(pthread_mutex_t *m) {
    struct pthread *self = __pthread_self();
    int r = relock(m, self->tid);
    if (r >= 0) {
        return r == EDEADLK ? EBUSY : r;
    }
    if (m->__type & ROBUST) {
        return robust_lock(m, self, 1, CLOCK_REALTIME, nullptr);
    }
    if (__sync_val_compare_and_swap(&m->__lock, 0, 1) != 0) {
        return EBUSY;
    }
    m->__owner = self->tid;
    m->__count = 0;
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *m) {
    struct pthread *self = __pthread_self();
    int type = m->__type;
    if ((type & KIND) != PTHREAD_MUTEX_NORMAL || (type & ROBUST)) {
        if (m->__owner != self->tid || !m->__lock) {
            return EPERM;
        }
        if ((type & KIND) == PTHREAD_MUTEX_RECURSIVE && m->__count) {
            m->__count--;
            return 0;
        }
    }
    m->__owner = 0;
    if (type & ROBUST) {
        self->robust.pending = ENTRY(m);
        robust_unlink(self, m);
        if (type & INCONSISTENT) {
            m->__type |= NOTRECOVERABLE;
        }
        unsigned old =
            (unsigned)__atomic_exchange_n(&m->__lock, 0, __ATOMIC_RELEASE);
        self->robust.pending = nullptr;
        if (old & FUTEX_WAITERS) {
            __wake_p(&m->__lock, m->__type & NOTRECOVERABLE ? INT32_MAX : 1,
                     shared(m));
        }
        return 0;
    }
    if (__atomic_exchange_n(&m->__lock, 0, __ATOMIC_RELEASE) == 2) {
        __wake_p(&m->__lock, 1, shared(m));
    }
    return 0;
}

int pthread_mutex_consistent(pthread_mutex_t *m) {
    if (!(m->__type & ROBUST) || !(m->__type & INCONSISTENT) ||
        m->__owner != __pthread_self()->tid) {
        return EINVAL;
    }
    m->__type &= ~INCONSISTENT;
    return 0;
}

void __pthread_robust_release(struct pthread *self) {
    void **head = &self->robust.list;
    while (*head && *head != head) {
        pthread_mutex_t *m = MUTEX(*head);
        *head = m->__next;
        m->__owner = 0;
        m->__count = 0;
        unsigned old;
        do {
            old = (unsigned)m->__lock;
        } while (!__sync_bool_compare_and_swap(
            &m->__lock, (int)old,
            (int)((old & FUTEX_WAITERS) | FUTEX_OWNER_DIED)));
        if (old & FUTEX_WAITERS) {
            __wake_p(&m->__lock, 1, shared(m));
        }
    }
}

int __mutex_release_all(pthread_mutex_t *m, int *count) {
    *count = m->__count;
    m->__count = 0;
    return pthread_mutex_unlock(m);
}

int __mutex_reacquire(pthread_mutex_t *m, int count) {
    int r = pthread_mutex_lock(m);
    if (r == 0 || r == EOWNERDEAD) {
        m->__count = count;
    }
    return r;
}
