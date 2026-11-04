#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_pid_t
#define __NEED_uid_t
#define __NEED_pthread_t
#define __NEED_pthread_attr_t
#define __NEED_sigset_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>

typedef int sig_atomic_t;

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGIOT    SIGABRT
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPOLL   SIGIO
#define SIGPWR    30
#define SIGSYS    31

#define SIGRTMIN 34
#define SIGRTMAX 64
#define NSIG     65

#define SIG_DFL  ((void (*)(int))0)
#define SIG_IGN  ((void (*)(int))1)
#define SIG_ERR  ((void (*)(int)) - 1)
#define SIG_HOLD ((void (*)(int))2)

#define SA_NOCLDSTOP 0x00000001
#define SA_NOCLDWAIT 0x00000002
#define SA_SIGINFO   0x00000004
#define SA_ONSTACK   0x08000000
#define SA_RESTART   0x10000000
#define SA_NODEFER   0x40000000
#define SA_RESETHAND 0x80000000

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SS_ONSTACK  1
#define SS_DISABLE  2
#define MINSIGSTKSZ 2048
#define SIGSTKSZ    8192

#define SI_USER    0
#define SI_KERNEL  0x80
#define SI_QUEUE   (-1)
#define SI_TIMER   (-2)
#define SI_MESGQ   (-3)
#define SI_ASYNCIO (-4)
#define SI_TKILL   (-6)

#define ILL_ILLOPC 1
#define ILL_ILLOPN 2
#define ILL_ILLADR 3
#define ILL_ILLTRP 4
#define ILL_PRVOPC 5
#define ILL_PRVREG 6
#define ILL_COPROC 7
#define ILL_BADSTK 8

#define FPE_INTDIV 1
#define FPE_INTOVF 2
#define FPE_FLTDIV 3
#define FPE_FLTOVF 4
#define FPE_FLTUND 5
#define FPE_FLTRES 6
#define FPE_FLTINV 7
#define FPE_FLTSUB 8

#define SEGV_MAPERR 1
#define SEGV_ACCERR 2

#define BUS_ADRALN 1
#define BUS_ADRERR 2
#define BUS_OBJERR 3

#define TRAP_BRKPT 1
#define TRAP_TRACE 2

#define CLD_EXITED    1
#define CLD_KILLED    2
#define CLD_DUMPED    3
#define CLD_TRAPPED   4
#define CLD_STOPPED   5
#define CLD_CONTINUED 6

#define POLL_IN  1
#define POLL_OUT 2
#define POLL_MSG 3
#define POLL_ERR 4
#define POLL_PRI 5
#define POLL_HUP 6

#define SIGEV_SIGNAL 0
#define SIGEV_NONE   1
#define SIGEV_THREAD 2

union sigval {
    int sival_int;
    void *sival_ptr;
};

typedef struct {
    int si_signo, si_errno, si_code;
    int __pad0;
    union {
        char __pad[112];
        struct {
            pid_t pid;
            uid_t uid;
            union sigval value;
        } __rt;
        struct {
            pid_t pid;
            uid_t uid;
            int status;
            long utime, stime;
        } __chld;
        struct {
            int tid;
            int overrun;
            union sigval value;
        } __timer;
        struct {
            void *addr;
            short addr_lsb;
        } __fault;
        struct {
            long band;
            int fd;
        } __poll;
    } __si;
} siginfo_t;

#define si_pid     __si.__rt.pid
#define si_uid     __si.__rt.uid
#define si_value   __si.__rt.value
#define si_status  __si.__chld.status
#define si_addr    __si.__fault.addr
#define si_band    __si.__poll.band
#define si_timerid __si.__timer.tid
#define si_overrun __si.__timer.overrun

struct sigevent {
    union sigval sigev_value;
    int sigev_signo;
    int sigev_notify;
    union {
        int __pad[12];
        struct {
            void (*function)(union sigval);
            pthread_attr_t *attributes;
        } __thr;
    } __sev;
};

#define sigev_notify_function   __sev.__thr.function
#define sigev_notify_attributes __sev.__thr.attributes

struct sigaction {
    union {
        void (*__handler)(int);
        void (*__sigaction)(int, siginfo_t *, void *);
    } __sa;
    sigset_t sa_mask;
    int sa_flags;
};

#define sa_handler   __sa.__handler
#define sa_sigaction __sa.__sigaction

typedef struct {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
} stack_t;

enum {
    REG_R8,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,
    REG_RDI,
    REG_RSI,
    REG_RBP,
    REG_RBX,
    REG_RDX,
    REG_RAX,
    REG_RCX,
    REG_RSP,
    REG_RIP,
    REG_EFL,
    REG_CSGSFS,
    REG_ERR,
    REG_TRAPNO,
    REG_OLDMASK,
    REG_CR2,
    NGREG
};

typedef long long greg_t, gregset_t[NGREG];

typedef struct {
    gregset_t gregs;
    void *fpregs; // the fxsave image
    unsigned long long __reserved[8];
} mcontext_t;

typedef struct ucontext_t {
    unsigned long uc_flags;
    struct ucontext_t *uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    sigset_t uc_sigmask;
} ucontext_t;

int kill(pid_t pid, int sig);
int killpg(pid_t pgrp, int sig);
void psiginfo(const siginfo_t *info, const char *s);
void psignal(int sig, const char *s);
int pthread_kill(pthread_t thread, int sig);
int pthread_sigmask(int how, const sigset_t *__restrict set,
                    sigset_t *__restrict old);
int raise(int sig);
int sighold(int sig);
int sigignore(int sig);
int siginterrupt(int sig, int flag);
int sigpause(int sig);
int sigrelse(int sig);
void (*sigset(int sig, void (*disp)(int)))(int);
int sig2str(int sig, char *str);
int sigaction(int sig, const struct sigaction *__restrict act,
              struct sigaction *__restrict old);
int sigaddset(sigset_t *set, int sig);
int sigaltstack(const stack_t *__restrict ss, stack_t *__restrict old);
int sigdelset(sigset_t *set, int sig);
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigismember(const sigset_t *set, int sig);
void (*signal(int sig, void (*handler)(int)))(int);
int sigpending(sigset_t *set);
int sigprocmask(int how, const sigset_t *__restrict set,
                sigset_t *__restrict old);
int sigqueue(pid_t pid, int sig, union sigval value);
int sigsuspend(const sigset_t *mask);
int sigtimedwait(const sigset_t *__restrict set, siginfo_t *__restrict info,
                 const struct timespec *__restrict timeout);
int sigwait(const sigset_t *__restrict set, int *__restrict sig);
int sigwaitinfo(const sigset_t *__restrict set, siginfo_t *__restrict info);
int str2sig(const char *__restrict str, int *__restrict sig);

#define SIG2STR_MAX 32

#ifdef __cplusplus
}
#endif
