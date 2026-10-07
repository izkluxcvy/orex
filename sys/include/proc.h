#pragma once

#include <stddef.h>
#include <stdint.h>

struct thread;
struct vmspace;

typedef int pid_t;

#define PROC_NAME_MAX 16

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
};

extern struct proc proc0;

struct proc *curproc();

struct proc      *proc_spawn_blob(const void *code, size_t len);
int               proc_join(struct proc *p);
[[noreturn]] void proc_exit(int status);
