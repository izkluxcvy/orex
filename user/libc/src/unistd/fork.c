#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread_impl.h>
#include <stdio_impl.h>
#include <syscall.h>

void __malloc_fork(int phase);
void __stdio_fork_child();

struct atfork {
    void (*prepare)();
    void (*parent)();
    void (*child)();
    struct atfork *prev, *next;
};

static struct atfork *first, *last;
static volatile int lock;

int pthread_atfork(void (*prepare)(), void (*parent)(), void (*child)()) {
    struct atfork *a = malloc(sizeof(*a));
    if (!a) {
        return ENOMEM;
    }
    *a = (struct atfork){prepare, parent, child, nullptr, nullptr};
    __lock(&lock);
    a->prev = last;
    if (last) {
        last->next = a;
    } else {
        first = a;
    }
    last = a;
    __unlock(&lock);
    return 0;
}

pid_t _Fork() {
    long pid = __syscall0(SYS_fork);
    if (pid == 0) {
        struct pthread *self = __pthread_self();
        self->tid = (int)__syscall1(SYS_set_tid_address, (long)&self->tid);
        __pthread_robust_init(self);
        __libc_threads.threads_minus_1 = 0;
    }
    return (pid_t)__syscall_ret(pid);
}

pid_t fork() {
    __lock(&lock);
    for (struct atfork *a = last; a; a = a->prev) {
        if (a->prepare) {
            a->prepare();
        }
    }
    __ofl_lock();
    __malloc_fork(0);
    long pid = __syscall0(SYS_fork);
    if (pid == 0) {
        struct pthread *self = __pthread_self();
        self->tid = (int)__syscall1(SYS_set_tid_address, (long)&self->tid);
        __pthread_robust_init(self);
        __libc_threads.threads_minus_1 = 0;
        __malloc_fork(2);
        __stdio_fork_child();
        lock = 0;
    } else {
        __malloc_fork(1);
        __ofl_unlock();
    }
    for (struct atfork *a = first; a; a = a->next) {
        void (*fn)() = pid == 0 ? a->child : a->parent;
        if (fn) {
            fn();
        }
    }
    if (pid != 0) {
        __unlock(&lock);
    }
    return (pid_t)__syscall_ret(pid);
}
