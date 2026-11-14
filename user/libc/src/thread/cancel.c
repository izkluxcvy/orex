#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <pthread_impl.h>

long __syscall_cp_asm(volatile int *cancel, long n, long a, long b, long c,
                      long d, long e, long f);
extern const char __cp_begin[], __cp_end[], __cp_cancel[];

__asm__(".text\n"
        ".global __syscall_cp_asm\n"
        ".type __syscall_cp_asm,@function\n"
        "__syscall_cp_asm:\n"
        ".global __cp_begin\n"
        "__cp_begin:\n"
        "  mov eax, [rdi]\n"
        "  test eax, eax\n"
        "  jnz __cp_cancel\n"
        "  mov rax, rsi\n"
        "  mov rdi, rdx\n"
        "  mov rsi, rcx\n"
        "  mov rdx, r8\n"
        "  mov r10, r9\n"
        "  mov r8, [rsp + 8]\n"
        "  mov r9, [rsp + 16]\n"
        "  syscall\n"
        ".global __cp_end\n"
        "__cp_end:\n"
        "  ret\n"
        ".global __cp_cancel\n"
        "__cp_cancel:\n"
        "  jmp __cancel\n");

long __cancel() {
    struct pthread *self = __pthread_self();
    if (!self->cancel_disable) {
        pthread_exit(PTHREAD_CANCELED);
    }
    return -ECANCELED;
}

void __pthread_exit_cancel() { pthread_exit(PTHREAD_CANCELED); }

long __syscall_cp(long n, long a, long b, long c, long d, long e, long f) {
    struct pthread *self = __pthread_self();
    if (self->cancel_disable) {
        return __syscall6(n, a, b, c, d, e, f);
    }
    long r = __syscall_cp_asm(&self->cancel, n, a, b, c, d, e, f);
    if (r == -EINTR && n != SYS_close && self->cancel &&
        !self->cancel_disable) {
        r = __cancel();
    }
    return r;
}

static void cancel_handler(int sig, siginfo_t *si, void *ctx) {
    (void)sig, (void)si;
    struct pthread *self = __pthread_self();
    ucontext_t *uc = ctx;
    uintptr_t pc = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
    if (!self->cancel || self->cancel_disable) {
        return;
    }
    if (self->cancel_async) {
        __syscall4(SYS_rt_sigprocmask, SIG_SETMASK, (long)&uc->uc_sigmask, 0,
                   8);
        __cancel();
    }
    if (pc >= (uintptr_t)__cp_begin && pc < (uintptr_t)__cp_end) {
        uc->uc_mcontext.gregs[REG_RIP] = (long long)(uintptr_t)__cp_cancel;
    }
}

int __sigaction_internal(int sig, const struct sigaction *act,
                         struct sigaction *old);

static void init_cancellation() {
    static volatile int done;
    if (done) {
        return;
    }
    struct sigaction sa = {.sa_sigaction = cancel_handler,
                           .sa_flags = SA_SIGINFO | SA_RESTART};
    sigfillset(&sa.sa_mask);
    __sigaction_internal(SIGCANCEL, &sa, nullptr);
    done = 1;
}

int pthread_cancel(pthread_t th) {
    struct pthread *t = (struct pthread *)th;
    init_cancellation();
    t->cancel = 1;
    if (t == __pthread_self()) {
        if (!t->cancel_disable && t->cancel_async) {
            __cancel();
        }
        return 0;
    }
    int tid = t->tid;
    if (tid) {
        __syscall3(SYS_tgkill, getpid(), tid, SIGCANCEL);
    }
    return 0;
}

void pthread_testcancel() {
    struct pthread *self = __pthread_self();
    if (self->cancel && !self->cancel_disable) {
        __cancel();
    }
}

int pthread_setcancelstate(int state, int *old) {
    if (state != PTHREAD_CANCEL_ENABLE && state != PTHREAD_CANCEL_DISABLE) {
        return EINVAL;
    }
    struct pthread *self = __pthread_self();
    if (old) {
        *old = self->cancel_disable;
    }
    self->cancel_disable = state;
    return 0;
}

int pthread_setcanceltype(int type, int *old) {
    if (type != PTHREAD_CANCEL_DEFERRED &&
        type != PTHREAD_CANCEL_ASYNCHRONOUS) {
        return EINVAL;
    }
    struct pthread *self = __pthread_self();
    if (old) {
        *old = self->cancel_async;
    }
    self->cancel_async = type;
    if (type == PTHREAD_CANCEL_ASYNCHRONOUS) {
        pthread_testcancel();
    }
    return 0;
}
