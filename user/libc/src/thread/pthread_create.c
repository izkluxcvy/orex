#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <libc.h>
#include <pthread_impl.h>

#define CLONE_VM             0x00000100
#define CLONE_FS             0x00000200
#define CLONE_FILES          0x00000400
#define CLONE_SIGHAND        0x00000800
#define CLONE_THREAD         0x00010000
#define CLONE_SYSVSEM        0x00040000
#define CLONE_SETTLS         0x00080000
#define CLONE_PARENT_SETTID  0x00100000
#define CLONE_CHILD_CLEARTID 0x00200000

#define DEFAULT_STACK (1UL << 20)
#define PAGE          4096UL

struct attr {
    size_t stacksize, guardsize;
    void *stackaddr;
    int detach, policy, prio, inherit, scope, set;
};
static_assert(sizeof(struct attr) <= sizeof(pthread_attr_t));

static struct attr defaults() {
    return (struct attr){.stacksize = DEFAULT_STACK, .guardsize = PAGE};
}

int __clone(int (*fn)(void *), void *stack, int flags, void *arg, int *ptid,
            void *tls, int *ctid);
__asm__(".text\n"
        ".global __clone\n"
        ".type __clone,@function\n"
        "__clone:\n"
        "  and rsi, -16\n"
        "  sub rsi, 16\n"
        "  mov [rsi + 8], rcx\n" // arg
        "  mov [rsi], rdi\n"     // fn
        "  mov r10, [rsp + 8]\n" // ctid
        "  mov rdi, rdx\n"       // flags
        "  mov rdx, r8\n"        // ptid
        "  mov r8, r9\n"         // tls
        "  mov eax, 56\n"        // SYS_clone
        "  syscall\n"
        "  test eax, eax\n"
        "  jnz 1f\n"
        "  xor ebp, ebp\n"
        "  pop rax\n"
        "  pop rdi\n"
        "  call rax\n"
        "  mov edi, eax\n"
        "  mov rax, 60\n" // SYS_exit
        "  syscall\n"
        "  hlt\n"
        "1:\n"
        "  ret\n");

static int start(void *arg) {
    struct pthread *self = arg;
    __pthread_robust_init(self);
    while (self->sched_hold == 1) {
        __timedwait(&self->sched_hold, 1, CLOCK_MONOTONIC, nullptr, 0);
    }
    __syscall4(SYS_rt_sigprocmask, SIG_SETMASK, (long)&self->start_mask, 0, 8);
    pthread_exit(self->sched_hold ? nullptr : self->start(self->arg));
}

int pthread_create(pthread_t *__restrict res,
                   const pthread_attr_t *__restrict ap, void *(*fn)(void *),
                   void *__restrict arg) {
    struct attr a = ap ? *(const struct attr *)ap : defaults();
    if (a.inherit == PTHREAD_EXPLICIT_SCHED && a.policy != SCHED_OTHER &&
        __syscall0(SYS_geteuid) != 0) {
        return EPERM;
    }
    size_t tls = (__tls_area_size() + PAGE - 1) & ~(PAGE - 1);
    size_t guard = a.stackaddr ? 0 : (a.guardsize + PAGE - 1) & ~(PAGE - 1);
    size_t stack = a.stackaddr ? 0 : (a.stacksize + PAGE - 1) & ~(PAGE - 1);
    size_t size = guard + stack + tls;

    char *map = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED) {
        return EAGAIN;
    }
    if (guard && mprotect(map, guard, PROT_NONE) != 0) {
        munmap(map, size);
        return EAGAIN;
    }
    struct pthread *self = __pthread_self();
    struct pthread *t = __tls_place(map + guard + stack, tls);
    t->tcb.stack_guard = self->tcb.stack_guard;
    t->tcb.pointer_guard = self->tcb.pointer_guard;
    t->start = fn;
    t->arg = arg;
    t->detach = a.detach ? DT_DETACHED : DT_JOINABLE;
    t->map_base = map;
    t->map_size = size;
    t->sched_hold = a.inherit == PTHREAD_EXPLICIT_SCHED;
    char *top =
        a.stackaddr ? (char *)a.stackaddr + a.stacksize : map + guard + stack;

    __libc_threads.threaded = 1;
    __atomic_fetch_add(&__libc_threads.threads_minus_1, 1, __ATOMIC_SEQ_CST);

    sigset_t all, old;
    sigfillset(&all);
    __syscall4(SYS_rt_sigprocmask, SIG_BLOCK, (long)&all, (long)&old, 8);
    memcpy(&t->start_mask, &old, sizeof(t->start_mask));
    int r = __clone(start, top,
                    CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
                        CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
                        CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID,
                    t, (int *)&t->tid, t, (int *)&t->tid);
    __syscall4(SYS_rt_sigprocmask, SIG_SETMASK, (long)&old, 0, 8);
    if (r < 0) {
        __atomic_fetch_sub(&__libc_threads.threads_minus_1, 1,
                           __ATOMIC_SEQ_CST);
        munmap(map, size);
        return EAGAIN;
    }
    if (a.inherit == PTHREAD_EXPLICIT_SCHED) {
        struct sched_param sp = {a.prio};
        long err = __syscall3(SYS_sched_setscheduler, r, a.policy, (long)&sp);
        t->sched_hold = err < 0 ? 2 : 0;
        __wake(&t->sched_hold, 1);
        if (err < 0) {
            pthread_join((pthread_t)t, nullptr);
            return (int)-err;
        }
    }
    *res = (pthread_t)t;
    return 0;
}

[[noreturn]] static void unmap_self(void *base, size_t size) {
    __asm__ __volatile__("mov eax, 11\n" // SYS_munmap
                         "syscall\n"
                         "xor edi, edi\n"
                         "mov eax, 60\n" // SYS_exit
                         "syscall"
                         :
                         : "D"(base), "S"(size)
                         : "rax", "rcx", "r11", "memory");
    __builtin_unreachable();
}

void pthread_exit(void *result) {
    struct pthread *self = __pthread_self();
    self->cancel_disable = 1;
    self->result = result;
    while (self->cleanup) {
        struct __ptcb *cb = self->cleanup;
        self->cleanup = cb->__next;
        cb->__f(cb->__x);
    }
    if (__cxa_thread_finalize) {
        __cxa_thread_finalize();
    }
    __pthread_tsd_run_dtors(self);
    if (__tls_free_dynamic) {
        __tls_free_dynamic(self);
    }
    __pthread_robust_release(self);

    if (__atomic_fetch_sub(&__libc_threads.threads_minus_1, 1,
                           __ATOMIC_SEQ_CST) == 0) {
        __libc_threads.threads_minus_1 = 0;
        exit(0);
    }
    sigset_t all;
    sigfillset(&all);
    __syscall4(SYS_rt_sigprocmask, SIG_BLOCK, (long)&all, 0, 8);

    int old =
        __sync_val_compare_and_swap(&self->detach, DT_JOINABLE, DT_EXITING);
    if (old == DT_DETACHED && self->map_base) {
        __syscall1(SYS_set_tid_address, 0);
        __syscall2(SYS_set_robust_list, 0, 24);
        unmap_self(self->map_base, self->map_size);
    }
    while (1) {
        __syscall1(SYS_exit, 0);
    }
}

int pthread_join(pthread_t th, void **result) {
    struct pthread *t = (struct pthread *)th;
    if (t == __pthread_self()) {
        return EDEADLK;
    }
    if (t->detach == DT_DETACHED) {
        return EINVAL;
    }
    pthread_testcancel();
    int tid;
    while ((tid = t->tid) != 0) {
        if (__timedwait(&t->tid, tid, CLOCK_MONOTONIC, nullptr, 1) == EINVAL) {
            return EINVAL;
        }
    }
    if (result) {
        *result = t->result;
    }
    if (t->map_base) {
        munmap(t->map_base, t->map_size);
    }
    return 0;
}

int pthread_detach(pthread_t th) {
    struct pthread *t = (struct pthread *)th;
    int old = __sync_val_compare_and_swap(&t->detach, DT_JOINABLE, DT_DETACHED);
    if (old == DT_DETACHED) {
        return EINVAL;
    }
    if (old == DT_EXITING) {
        return pthread_join(th, nullptr);
    }
    return 0;
}

pthread_t pthread_self() { return (pthread_t)__pthread_self(); }

int pthread_equal(pthread_t a, pthread_t b) { return a == b; }

void _pthread_cleanup_push(struct __ptcb *cb, void (*f)(void *), void *x) {
    struct pthread *self = __pthread_self();
    cb->__f = f;
    cb->__x = x;
    cb->__next = self->cleanup;
    self->cleanup = cb;
}

void _pthread_cleanup_pop(struct __ptcb *cb, int run) {
    __pthread_self()->cleanup = cb->__next;
    if (run) {
        cb->__f(cb->__x);
    }
}

/*
 * attributes
 */

#define A(a)  ((struct attr *)(a))
#define CA(a) ((const struct attr *)(a))

int pthread_attr_init(pthread_attr_t *a) {
    memset(a, 0, sizeof(*a));
    *A(a) = defaults();
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *a) {
    (void)a;
    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *a, int *state) {
    *state = CA(a)->detach;
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *a, int state) {
    if (state != PTHREAD_CREATE_JOINABLE && state != PTHREAD_CREATE_DETACHED) {
        return EINVAL;
    }
    A(a)->detach = state;
    return 0;
}

int pthread_attr_getguardsize(const pthread_attr_t *__restrict a,
                              size_t *__restrict size) {
    *size = CA(a)->guardsize;
    return 0;
}

int pthread_attr_setguardsize(pthread_attr_t *a, size_t size) {
    if (size > SIZE_MAX / 2) {
        return EINVAL;
    }
    A(a)->guardsize = size;
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *__restrict a,
                              size_t *__restrict size) {
    *size = CA(a)->stacksize;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *a, size_t size) {
    if (size < PTHREAD_STACK_MIN || size > SIZE_MAX / 4) {
        return EINVAL;
    }
    A(a)->stackaddr = nullptr;
    A(a)->stacksize = size;
    return 0;
}

int pthread_attr_getstack(const pthread_attr_t *__restrict a,
                          void **__restrict addr, size_t *__restrict size) {
    if (!CA(a)->stackaddr) {
        return EINVAL;
    }
    *addr = CA(a)->stackaddr;
    *size = CA(a)->stacksize;
    return 0;
}

int pthread_attr_setstack(pthread_attr_t *a, void *addr, size_t size) {
    if (size < PTHREAD_STACK_MIN || !addr) {
        return EINVAL;
    }
    A(a)->stackaddr = addr;
    A(a)->stacksize = size;
    return 0;
}

int pthread_attr_getinheritsched(const pthread_attr_t *__restrict a,
                                 int *__restrict inherit) {
    *inherit = CA(a)->inherit;
    return 0;
}

int pthread_attr_setinheritsched(pthread_attr_t *a, int inherit) {
    if (inherit != PTHREAD_INHERIT_SCHED && inherit != PTHREAD_EXPLICIT_SCHED) {
        return EINVAL;
    }
    A(a)->inherit = inherit;
    return 0;
}

int pthread_attr_getschedpolicy(const pthread_attr_t *__restrict a,
                                int *__restrict policy) {
    *policy = CA(a)->policy;
    return 0;
}

int pthread_attr_getschedparam(const pthread_attr_t *__restrict a,
                               struct sched_param *__restrict p) {
    p->sched_priority = CA(a)->prio;
    return 0;
}

int pthread_attr_setschedparam(pthread_attr_t *__restrict a,
                               const struct sched_param *__restrict p) {
    if (p->sched_priority < 0 || p->sched_priority > 99) {
        return EINVAL;
    }
    A(a)->prio = p->sched_priority;
    return 0;
}

int pthread_attr_getscope(const pthread_attr_t *__restrict a,
                          int *__restrict scope) {
    *scope = CA(a)->scope;
    return 0;
}

int pthread_attr_setscope(pthread_attr_t *a, int scope) {
    if (scope == PTHREAD_SCOPE_PROCESS) {
        return ENOTSUP;
    }
    if (scope != PTHREAD_SCOPE_SYSTEM) {
        return EINVAL;
    }
    A(a)->scope = scope;
    return 0;
}

/*
 * scheduling
 */

int pthread_getschedparam(pthread_t th, int *__restrict policy,
                          struct sched_param *__restrict p) {
    int tid = ((struct pthread *)th)->tid;
    long r = __syscall1(SYS_sched_getscheduler, tid);
    if (r < 0) {
        return (int)-r;
    }
    *policy = (int)r;
    return (int)-__syscall2(SYS_sched_getparam, tid, (long)p);
}

int pthread_setschedparam(pthread_t th, int policy,
                          const struct sched_param *p) {
    return (int)-__syscall3(SYS_sched_setscheduler, ((struct pthread *)th)->tid,
                            policy, (long)p);
}

int pthread_setschedprio(pthread_t th, int prio) {
    struct sched_param p = {prio};
    return (int)-__syscall2(SYS_sched_setparam, ((struct pthread *)th)->tid,
                            (long)&p);
}

static int concurrency;
int pthread_getconcurrency() { return concurrency; }

int pthread_setconcurrency(int level) {
    if (level < 0) {
        return EINVAL;
    }
    concurrency = level;
    return 0;
}

int pthread_getcpuclockid(pthread_t th, clockid_t *clock) {
    struct pthread *t = (struct pthread *)th;
    if (!t->tid) {
        return ESRCH;
    }
    *clock = (clockid_t)((~(unsigned)t->tid << 3) | 6);
    return 0;
}

int sched_yield() { return (int)__syscall_ret(__syscall0(SYS_sched_yield)); }
int sched_get_priority_max(int policy) {
    return (int)__syscall_ret(__syscall1(SYS_sched_get_priority_max, policy));
}
int sched_get_priority_min(int policy) {
    return (int)__syscall_ret(__syscall1(SYS_sched_get_priority_min, policy));
}
