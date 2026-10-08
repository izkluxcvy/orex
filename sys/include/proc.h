#pragma once

#include <stddef.h>
#include <stdint.h>

#include <waitq.h>

struct exec_args;
struct siginfo;
struct thread;
struct vmspace;
struct vnode;

typedef int pid_t;

#define PROC_NAME_MAX 16

#define WNOHANG    1
#define WUNTRACED  2
#define WEXITED    4
#define WCONTINUED 8
#define WNOWAIT    0x0100'0000

#define W_EXITCODE(code) (((code) & 0xff) << 8)
#define W_SIGNALED(sig)  ((sig) & 0x7f)

enum proc_state {
    PROC_ALIVE,
    PROC_ZOMBIE,
};

struct proc {
    pid_t           pid;
    char            name[PROC_NAME_MAX];
    enum proc_state state;
    int             status;

    struct vmspace *vm;
    struct thread  *threads;
    int             nthreads;

    struct proc *parent;
    struct proc *children;
    struct proc *sibling;
    struct waitq child_exit;
    struct proc *all_next;
};

extern struct proc proc0;

extern struct proc *proc_list;

struct proc *curproc();

struct proc *proc_spawn(const char *path, const char *const *argv,
                        const char *const *envp);
int proc_exec(const char *path, struct vnode *given, const struct exec_args *a);
pid_t             proc_fork();
[[noreturn]] void proc_exit(int status);
[[noreturn]] void proc_thread_exit(int status);
pid_t proc_wait(pid_t pid, int *status, int options, struct siginfo *info,
                void *rusage);
