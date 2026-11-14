#pragma once

#include <stddef.h>
#include <stdint.h>

#include <cred.h>
#include <ktime.h>
#include <signal.h>
#include <spinlock.h>
#include <vfs.h>
#include <waitq.h>

struct exec_args;
struct file;
struct ktimer;
struct sigq;
struct thread;
struct tty;
struct vmspace;

typedef int pid_t;

#define PROC_NAME_MAX 16
#define PROC_FD_MAX   256

#define WNOHANG    1
#define WUNTRACED  2
#define WEXITED    4
#define WCONTINUED 8
#define WNOWAIT    0x0100'0000

#define W_EXITCODE(code) (((code) & 0xff) << 8)
#define W_SIGNALED(sig)  ((sig) & 0x7f)

#define CLONE_VM             0x00000100
#define CLONE_FS             0x00000200
#define CLONE_FILES          0x00000400
#define CLONE_SIGHAND        0x00000800
#define CLONE_VFORK          0x00004000
#define CLONE_THREAD         0x00010000
#define CLONE_SYSVSEM        0x00040000
#define CLONE_SETTLS         0x00080000
#define CLONE_PARENT_SETTID  0x00100000
#define CLONE_CHILD_CLEARTID 0x00200000
#define CLONE_DETACHED       0x00400000
#define CLONE_CHILD_SETTID   0x01000000

#define EXIT_GROUP 1
#define EXIT_EXEC  2

enum proc_state {
    PROC_ALIVE,
    PROC_ZOMBIE,
};

struct proc {
    pid_t pid;
    pid_t pgid;
    pid_t sid;
    struct tty *ctty;
    int execed;
    char name[PROC_NAME_MAX];
    int64_t start_time;
    enum proc_state state;
    int status;

    struct vmspace *vm;
    struct thread *threads;
    int nthreads;
    int exiting;
    int exit_status;
    struct waitq exec_wait;
    struct spinlock files_lock;
    struct file *fds[PROC_FD_MAX];
    uint8_t fd_flags[PROC_FD_MAX];
    struct vnode *cwd;
    struct cred cred;
    uint32_t umask;

    struct proc *parent;
    struct proc *children;
    struct proc *sibling;
    struct waitq child_exit;
    struct proc *all_next;

    uint64_t sig_pending;
    struct k_sigaction sig_actions[NSIG];
    struct siginfo sig_info[SIGRTMIN];
    struct sigq *sig_queue;
    int sig_queued;
    int stopped, stop_sig;
    int stop_report, cont_report;
    struct waitq stop_wait;

    uint64_t utime, stime;
    uint64_t cutime, cstime;
    uint64_t cpu_ns;
    struct ktimer *itimer_real;
    uint64_t itimer_left[3], itimer_interval[3];
    struct ktimer *timers[TIMER_MAX];
};

extern struct proc proc0;

extern struct proc *proc_list;

struct proc *curproc();

struct proc *proc_spawn(const char *path, const char *const *argv,
                        const char *const *envp);
int proc_exec(const char *path, struct vnode *given, const struct exec_args *a);
pid_t proc_fork();
long proc_clone_thread(unsigned long flags, uintptr_t sp, int *uptid,
                       int *uctid, uintptr_t tls);
[[noreturn]] void proc_exit(int status);
[[noreturn]] void proc_thread_exit(int status);
struct thread *proc_find_thread_locked(pid_t tid);
pid_t proc_wait(pid_t pid, int *status, int options, struct siginfo *info,
                void *rusage);

struct proc *proc_find_locked(pid_t pid);
int proc_setpgid(pid_t pid, pid_t pgid);
pid_t proc_setsid();
pid_t proc_getsid(pid_t pid);
int pgrp_orphaned_locked(pid_t pgid);
pid_t proc_getpgid(pid_t pid);
