#pragma once

#include <stdint.h>

struct proc;
struct sigq;
struct thread;

#define NSIG     65
#define SIGRTMIN 32

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
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
#define SIGPWR    30
#define SIGSYS    31

#define SIG_DFL 0
#define SIG_IGN 1

#define SA_NOCLDSTOP 0x00000001
#define SA_NOCLDWAIT 0x00000002
#define SA_SIGINFO   0x00000004
#define SA_RESTORER  0x04000000
#define SA_ONSTACK   0x08000000
#define SA_RESTART   0x10000000
#define SA_NODEFER   0x40000000
#define SA_RESETHAND 0x80000000

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SS_ONSTACK   1
#define SS_DISABLE   2
#define MINSIGSTKSZ  2048
#define SIGQUEUE_MAX 256 // queued real-time signals per process

#define SIGBIT(sig) (1ULL << ((sig) - 1))

// si_code values.
#define SI_USER       0
#define SI_KERNEL     0x80
#define SI_QUEUE      (-1)
#define SI_TKILL      (-6)
#define SI_TIMER      (-2)
#define SEGV_MAPERR   1
#define SEGV_ACCERR   2
#define BUS_ADRERR    2
#define ILL_ILLOPN    2
#define FPE_INTDIV    1
#define FPE_FLTINV    7
#define TRAP_BRKPT    1
#define CLD_EXITED    1
#define CLD_KILLED    2
#define CLD_STOPPED   5
#define CLD_CONTINUED 6

struct k_sigaction {
    uintptr_t handler;
    uint64_t  flags;
    uintptr_t restorer;
    uint64_t  mask;
};

struct siginfo {
    int32_t si_signo, si_errno, si_code, pad;
    union {
        struct {
            int32_t  pid;
            uint32_t uid;
            union {
                int32_t  status;
                uint64_t value;
            };
        };
        uintptr_t addr;
        uint8_t   bytes[112];
    };
};

static_assert(sizeof(struct siginfo) == 128);

struct k_stack {
    uintptr_t sp;
    int32_t   flags;
    uint32_t  pad;
    uint64_t  size;
};

struct thread_sig {
    uint64_t       pending;
    uint64_t       blocked;
    uint64_t       saved;
    int            restore;
    uint64_t       waitset;
    struct k_stack altstack;
    struct siginfo info[SIGRTMIN];
    struct sigq   *queue;
};

#define ERESTARTSYS    512
#define ERESTARTNOINTR 513
#define ERESTARTNOHAND 514

void signal_send(struct proc *p, int sig);
void signal_send_locked(struct proc *p, int sig);
int  signal_send_info_locked(struct proc *p, const struct siginfo *info);
int  signal_send_thread_locked(struct thread *t, const struct siginfo *info);
int  signal_group_locked(int pgid, int sig);

void signal_force(int sig, int code, uintptr_t addr);

int  signal_pending();
int  signal_killed();
void signal_mask_during(uint64_t mask);
void signal_mask_end(long ret);
void signal_deliver(void *frame);

void signal_thread_init(struct thread *t, struct thread *from);
void signal_thread_exit(struct thread *t);
void signal_fork(struct proc *child, struct proc *parent);
void signal_exec(struct proc *p);
void signal_exit(struct proc *p);
int  signal_ignored_or_blocked(struct proc *p, int sig);
void signal_kick_others_locked(struct proc *p);

long sys_kill(long pid, long sig);
long sys_rt_sigaction(long sig, const struct k_sigaction *uact,
                      struct k_sigaction *uold, long setsize);
long sys_rt_sigprocmask(long how, const uint64_t *uset, uint64_t *uold,
                        long setsize);
long sys_rt_sigreturn();
long sys_rt_sigpending(uint64_t *uset, long setsize);
long sys_rt_sigsuspend(const uint64_t *umask, long setsize);
long sys_rt_sigtimedwait(const uint64_t *uset, struct siginfo *uinfo,
                         const void *utimeout, long setsize);
long sys_rt_sigqueueinfo(long pid, long sig, const struct siginfo *uinfo);
long sys_sigaltstack(const struct k_stack *uss, struct k_stack *uold);
long sys_tgkill(long tgid, long tid, long sig);
long sys_pause();
