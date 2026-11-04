#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "pthread_impl.h"
#include "syscall.h"

int kill(pid_t pid, int sig) {
    return (int)__syscall_ret(__syscall2(SYS_kill, pid, sig));
}

int killpg(pid_t pgrp, int sig) {
    if (pgrp < 0) {
        errno = EINVAL;
        return -1;
    }
    return kill(-pgrp, sig);
}

int raise(int sig) {
    return pthread_kill(pthread_self(), sig) ? (errno = EINVAL, -1) : 0;
}

int pthread_kill(pthread_t thread, int sig) {
    int tid = ((struct pthread *)thread)->tid;
    if (!tid) {
        return ESRCH;
    }
    return (int)-__syscall3(SYS_tgkill, getpid(), tid, sig);
}

int pthread_sigmask(int how, const sigset_t *__restrict set,
                    sigset_t *__restrict old) {
    sigset_t own;
    if (set && how != SIG_UNBLOCK) {
        own = *set;
        own.__bits[0] &= ~(1UL << (SIGCANCEL - 1));
        set = &own;
    }
    return (int)-__syscall4(SYS_rt_sigprocmask, how, (long)set, (long)old, 8);
}

int sigprocmask(int how, const sigset_t *__restrict set,
                sigset_t *__restrict old) {
    int r = pthread_sigmask(how, set, old);
    return r ? (errno = r, -1) : 0;
}

int sigpending(sigset_t *set) {
    return (int)__syscall_ret(__syscall2(SYS_rt_sigpending, (long)set, 8));
}

int sigsuspend(const sigset_t *mask) {
    sigset_t own = *mask;
    own.__bits[0] &= ~(1UL << (SIGCANCEL - 1));
    return (int)__syscall_ret(
        __syscall_cp(SYS_rt_sigsuspend, (long)&own, 8, 0, 0, 0, 0));
}

int pause(void) {
    return (int)__syscall_ret(__syscall_cp(SYS_pause, 0, 0, 0, 0, 0, 0));
}

int sigtimedwait(const sigset_t *__restrict set, siginfo_t *__restrict info,
                 const struct timespec *__restrict timeout) {
    return (int)__syscall_ret(
        __syscall_cp4(SYS_rt_sigtimedwait, set, info, timeout, 8));
}

int sigwaitinfo(const sigset_t *__restrict set, siginfo_t *__restrict info) {
    return sigtimedwait(set, info, NULL);
}

int sigwait(const sigset_t *__restrict set, int *__restrict sig) {
    long r;
    do {
        r = __syscall_cp4(SYS_rt_sigtimedwait, set, 0, 0, 8);
    } while (r == -EINTR);
    if (r < 0) {
        return (int)-r;
    }
    *sig = (int)r;
    return 0;
}

int sigqueue(pid_t pid, int sig, union sigval value) {
    siginfo_t info;
    memset(&info, 0, sizeof(info));
    info.si_signo = sig;
    info.si_code = SI_QUEUE;
    info.si_pid = getpid();
    info.si_uid = getuid();
    info.si_value = value;
    return (int)__syscall_ret(
        __syscall3(SYS_rt_sigqueueinfo, pid, sig, (long)&info));
}

int sigaltstack(const stack_t *__restrict ss, stack_t *__restrict old) {
    if (ss && !(ss->ss_flags & SS_DISABLE) && ss->ss_size < MINSIGSTKSZ) {
        errno = ENOMEM;
        return -1;
    }
    return (int)__syscall_ret(__syscall2(SYS_sigaltstack, (long)ss, (long)old));
}
