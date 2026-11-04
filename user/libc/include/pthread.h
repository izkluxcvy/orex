#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_clockid_t
#define __NEED_time_t
#define __NEED_struct_timespec
#define __NEED_pthread_t
#define __NEED_pthread_attr_t
#define __NEED_pthread_mutex_t
#define __NEED_pthread_cond_t
#define __NEED_pthread_rwlock_t
#define __NEED_pthread_barrier_t
#define __NEED_pthread_attrs
#include <bits/alltypes.h>

#include <sched.h>
#include <time.h>

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#define PTHREAD_MUTEX_NORMAL     0
#define PTHREAD_MUTEX_RECURSIVE  1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT    PTHREAD_MUTEX_NORMAL

#define PTHREAD_MUTEX_STALLED 0
#define PTHREAD_MUTEX_ROBUST  1

#define PTHREAD_PRIO_NONE    0
#define PTHREAD_PRIO_INHERIT 1
#define PTHREAD_PRIO_PROTECT 2

#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED  1

#define PTHREAD_INHERIT_SCHED  0
#define PTHREAD_EXPLICIT_SCHED 1

#define PTHREAD_SCOPE_SYSTEM  0
#define PTHREAD_SCOPE_PROCESS 1

#define PTHREAD_CANCEL_ENABLE       0
#define PTHREAD_CANCEL_DISABLE      1
#define PTHREAD_CANCEL_DEFERRED     0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1
#define PTHREAD_CANCELED            ((void *)-1)
#define PTHREAD_NULL                ((pthread_t)0)

#define PTHREAD_BARRIER_SERIAL_THREAD (-1)

#define PTHREAD_ONCE_INIT 0
#ifdef __cplusplus // all its fields, without a warning
#define PTHREAD_MUTEX_INITIALIZER                                              \
    {                                                                          \
    }
#define PTHREAD_COND_INITIALIZER                                               \
    {                                                                          \
    }
#define PTHREAD_RWLOCK_INITIALIZER                                             \
    {                                                                          \
    }
#else
#define PTHREAD_MUTEX_INITIALIZER  {0}
#define PTHREAD_COND_INITIALIZER   {0}
#define PTHREAD_RWLOCK_INITIALIZER {0}
#endif

// pthread_cleanup_push and _pop, a pair of macros around one block.
struct __ptcb {
    void (*__f)(void *);
    void *__x;
    struct __ptcb *__next;
};
void _pthread_cleanup_push(struct __ptcb *cb, void (*f)(void *), void *x);
void _pthread_cleanup_pop(struct __ptcb *cb, int run);
#define pthread_cleanup_push(f, x)                                             \
    do {                                                                       \
        struct __ptcb __cb;                                                    \
        _pthread_cleanup_push(&__cb, f, x);
#define pthread_cleanup_pop(r)                                                 \
    _pthread_cleanup_pop(&__cb, (r));                                          \
    }                                                                          \
    while (0)

int pthread_atfork(void (*prepare)(void), void (*parent)(void),
                   void (*child)(void));

int pthread_attr_destroy(pthread_attr_t *a);
int pthread_attr_getdetachstate(const pthread_attr_t *a, int *state);
int pthread_attr_getguardsize(const pthread_attr_t *__restrict a,
                              size_t *__restrict size);
int pthread_attr_getinheritsched(const pthread_attr_t *__restrict a,
                                 int *__restrict inherit);
int pthread_attr_getschedparam(const pthread_attr_t *__restrict a,
                               struct sched_param *__restrict param);
int pthread_attr_getschedpolicy(const pthread_attr_t *__restrict a,
                                int *__restrict policy);
int pthread_attr_getscope(const pthread_attr_t *__restrict a,
                          int *__restrict scope);
int pthread_attr_getstack(const pthread_attr_t *__restrict a,
                          void **__restrict addr, size_t *__restrict size);
int pthread_attr_getstacksize(const pthread_attr_t *__restrict a,
                              size_t *__restrict size);
int pthread_attr_init(pthread_attr_t *a);
int pthread_attr_setdetachstate(pthread_attr_t *a, int state);
int pthread_attr_setguardsize(pthread_attr_t *a, size_t size);
int pthread_attr_setinheritsched(pthread_attr_t *a, int inherit);
int pthread_attr_setschedparam(pthread_attr_t *__restrict a,
                               const struct sched_param *__restrict param);
int pthread_attr_setschedpolicy(pthread_attr_t *a, int policy);
int pthread_attr_setscope(pthread_attr_t *a, int scope);
int pthread_attr_setstack(pthread_attr_t *a, void *addr, size_t size);
int pthread_attr_setstacksize(pthread_attr_t *a, size_t size);

int pthread_barrier_destroy(pthread_barrier_t *b);
int pthread_barrier_init(pthread_barrier_t *__restrict b,
                         const pthread_barrierattr_t *__restrict a,
                         unsigned count);
int pthread_barrier_wait(pthread_barrier_t *b);
int pthread_barrierattr_destroy(pthread_barrierattr_t *a);
int pthread_barrierattr_getpshared(const pthread_barrierattr_t *__restrict a,
                                   int *__restrict pshared);
int pthread_barrierattr_init(pthread_barrierattr_t *a);
int pthread_barrierattr_setpshared(pthread_barrierattr_t *a, int pshared);

int pthread_cancel(pthread_t t);

int pthread_cond_broadcast(pthread_cond_t *c);
int pthread_cond_clockwait(pthread_cond_t *__restrict c,
                           pthread_mutex_t *__restrict m, clockid_t clock,
                           const struct timespec *__restrict abs);
int pthread_cond_destroy(pthread_cond_t *c);
int pthread_cond_init(pthread_cond_t *__restrict c,
                      const pthread_condattr_t *__restrict a);
int pthread_cond_signal(pthread_cond_t *c);
int pthread_cond_timedwait(pthread_cond_t *__restrict c,
                           pthread_mutex_t *__restrict m,
                           const struct timespec *__restrict abs);
int pthread_cond_wait(pthread_cond_t *__restrict c,
                      pthread_mutex_t *__restrict m);
int pthread_condattr_destroy(pthread_condattr_t *a);
int pthread_condattr_getclock(const pthread_condattr_t *__restrict a,
                              clockid_t *__restrict clock);
int pthread_condattr_getpshared(const pthread_condattr_t *__restrict a,
                                int *__restrict pshared);
int pthread_condattr_init(pthread_condattr_t *a);
int pthread_condattr_setclock(pthread_condattr_t *a, clockid_t clock);
int pthread_condattr_setpshared(pthread_condattr_t *a, int pshared);

int pthread_create(pthread_t *__restrict t, const pthread_attr_t *__restrict a,
                   void *(*start)(void *), void *__restrict arg);
int pthread_detach(pthread_t t);
int pthread_equal(pthread_t a, pthread_t b);
[[noreturn]] void pthread_exit(void *result);
int pthread_getconcurrency(void);
int pthread_getcpuclockid(pthread_t t, clockid_t *clock);
int pthread_getschedparam(pthread_t t, int *__restrict policy,
                          struct sched_param *__restrict param);
void *pthread_getspecific(pthread_key_t key);
int pthread_join(pthread_t t, void **result);
int pthread_key_create(pthread_key_t *key, void (*dtor)(void *));
int pthread_key_delete(pthread_key_t key);

int pthread_mutex_clocklock(pthread_mutex_t *__restrict m, clockid_t clock,
                            const struct timespec *__restrict abs);
int pthread_mutex_consistent(pthread_mutex_t *m);
int pthread_mutex_destroy(pthread_mutex_t *m);
int pthread_mutex_getprioceiling(const pthread_mutex_t *__restrict m,
                                 int *__restrict ceiling);
int pthread_mutex_init(pthread_mutex_t *__restrict m,
                       const pthread_mutexattr_t *__restrict a);
int pthread_mutex_lock(pthread_mutex_t *m);
int pthread_mutex_setprioceiling(pthread_mutex_t *__restrict m, int ceiling,
                                 int *__restrict old);
int pthread_mutex_timedlock(pthread_mutex_t *__restrict m,
                            const struct timespec *__restrict abs);
int pthread_mutex_trylock(pthread_mutex_t *m);
int pthread_mutex_unlock(pthread_mutex_t *m);
int pthread_mutexattr_destroy(pthread_mutexattr_t *a);
int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *__restrict a,
                                     int *__restrict ceiling);
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *__restrict a,
                                  int *__restrict protocol);
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *__restrict a,
                                 int *__restrict pshared);
int pthread_mutexattr_getrobust(const pthread_mutexattr_t *__restrict a,
                                int *__restrict robust);
int pthread_mutexattr_gettype(const pthread_mutexattr_t *__restrict a,
                              int *__restrict type);
int pthread_mutexattr_init(pthread_mutexattr_t *a);
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *a, int ceiling);
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *a, int protocol);
int pthread_mutexattr_setpshared(pthread_mutexattr_t *a, int pshared);
int pthread_mutexattr_setrobust(pthread_mutexattr_t *a, int robust);
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int type);

int pthread_once(pthread_once_t *once, void (*init)(void));

int pthread_rwlock_clockrdlock(pthread_rwlock_t *__restrict l, clockid_t clock,
                               const struct timespec *__restrict abs);
int pthread_rwlock_clockwrlock(pthread_rwlock_t *__restrict l, clockid_t clock,
                               const struct timespec *__restrict abs);
int pthread_rwlock_destroy(pthread_rwlock_t *l);
int pthread_rwlock_init(pthread_rwlock_t *__restrict l,
                        const pthread_rwlockattr_t *__restrict a);
int pthread_rwlock_rdlock(pthread_rwlock_t *l);
int pthread_rwlock_timedrdlock(pthread_rwlock_t *__restrict l,
                               const struct timespec *__restrict abs);
int pthread_rwlock_timedwrlock(pthread_rwlock_t *__restrict l,
                               const struct timespec *__restrict abs);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *l);
int pthread_rwlock_trywrlock(pthread_rwlock_t *l);
int pthread_rwlock_unlock(pthread_rwlock_t *l);
int pthread_rwlock_wrlock(pthread_rwlock_t *l);
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *a);
int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *__restrict a,
                                  int *__restrict pshared);
int pthread_rwlockattr_init(pthread_rwlockattr_t *a);
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *a, int pshared);

pthread_t pthread_self(void);
int pthread_setcancelstate(int state, int *old);
int pthread_setcanceltype(int type, int *old);
int pthread_setconcurrency(int level);
int pthread_setschedparam(pthread_t t, int policy,
                          const struct sched_param *param);
int pthread_setschedprio(pthread_t t, int prio);
int pthread_setspecific(pthread_key_t key, const void *value);

int pthread_spin_destroy(pthread_spinlock_t *l);
int pthread_spin_init(pthread_spinlock_t *l, int pshared);
int pthread_spin_lock(pthread_spinlock_t *l);
int pthread_spin_trylock(pthread_spinlock_t *l);
int pthread_spin_unlock(pthread_spinlock_t *l);

void pthread_testcancel(void);

#ifdef __cplusplus
}
#endif
