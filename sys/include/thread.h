#pragma once

#include <stddef.h>
#include <stdint.h>

#include <cpu.h>
#include <sched.h>
#include <waitq.h>

#define THREAD_NAME_MAX   16
#define THREAD_STACK_SIZE (16 * 1024)

struct mutex;
struct pmap;
struct proc;

typedef int tid_t;

typedef void *(*thread_entry_t)(void *);

enum thread_state {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_ZOMBIE,
};

struct thread {
    uint64_t rsp;

    tid_t tid;
    char  name[THREAD_NAME_MAX];

    enum thread_state state;
    int               policy;
    int               priority;
    int               base_priority;
    uint32_t          slice;

    thread_entry_t entry;
    void          *arg;
    void          *retval;

    void  *stack; // kernel stack
    size_t stack_size;

    struct mutex *blocked_on;
    struct mutex *held;

    struct proc   *proc;
    struct thread *proc_next;
    struct pmap   *pmap;
    void          *frame;
    uint64_t       cpu_ns;
    uint64_t       start_ns;
    int            timed_out;

    int           detached;
    struct waitq  joiners;
    struct waitq *sleep_wq;

    struct thread *next, *prev;
};

struct thread *thread_alloc(const char *name, thread_entry_t entry, void *arg,
                            int policy, int priority);
struct thread *thread_create(const char *name, thread_entry_t entry, void *arg,
                             int policy, int priority);
[[noreturn]] void thread_exit(void *retval);
[[noreturn]] void thread_exit_locked(void *retval);
int               thread_join(struct thread *t, void **retval);
int               thread_detach(struct thread *t);
void              thread_yield();
void              thread_reap();

static inline struct thread *thread_self() { return curthread; }
