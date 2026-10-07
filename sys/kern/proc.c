#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <kmalloc.h>
#include <memset.h>
#include <printf.h>
#include <proc.h>
#include <sched.h>
#include <spinlock.h>
#include <thread.h>
#include <vm.h>

#include "../mm/pmm.h"
#include "pmap.h"
#include "user.h"

struct proc  proc0    = {.name = "kernel"};
static pid_t next_pid = 1;

struct proc *curproc() { return curthread->proc ? curthread->proc : &proc0; }

static void set_name(struct proc *p, const char *path) {
    const char *base = path;
    for (const char *c = path; *c; c++) {
        if (*c == '/') {
            base = c + 1;
        }
    }
    int i = 0;
    for (; base[i] && i < PROC_NAME_MAX - 1; i++) {
        p->name[i] = base[i];
    }
    p->name[i] = '\0';
}

static struct proc *proc_alloc(const char *name) {
    struct proc *p = kmalloc(sizeof(*p));
    if (!p) {
        return nullptr;
    }
    memset(p, 0, sizeof(*p));

    set_name(p, name);

    p->vm = vm_create();
    if (!p->vm) {
        kfree(p);
        return nullptr;
    }
    return p;
}

static void proc_free(struct proc *p) {
    vm_destroy(p->vm);
    kfree(p);
}

static struct thread *new_thread(struct proc *p, thread_entry_t entry,
                                 void *arg) {
    struct thread *t = thread_alloc(p->name, entry, arg, SCHED_OTHER, 0);
    if (t) {
        t->proc     = p;
        t->pmap     = p->vm->pmap;
        t->detached = 1;
    }
    return t;
}

static struct thread *proc_thread(struct proc *p, thread_entry_t entry,
                                  void *arg) {
    struct thread *t = new_thread(p, entry, arg);
    if (t) {
        p->threads  = t;
        p->nthreads = 1;
    }
    return t;
}

static void proc_start(struct proc *p) {
    p->pid = next_pid++;
    sched_enqueue(p->threads);
}

struct user_entry {
    uintptr_t rip;
    uintptr_t rsp;
};

static void *user_start(void *arg) {
    struct user_entry e = *(struct user_entry *)arg;
    kfree(arg);
    usermode_enter(e.rip, e.rsp);
}

struct proc *proc_spawn_blob(const void *code, size_t len) {
    struct proc       *p = proc_alloc("blob");
    struct user_entry *e = p ? kmalloc(sizeof(*e)) : nullptr;
    if (!e) {
        if (p) {
            proc_free(p);
        }
        return nullptr;
    }

    uintptr_t base = 0x400000, top = p->vm->stack_top;
    int       rwx = PROT_READ | PROT_WRITE | PROT_EXEC;
    if (vm_map(p->vm, base, base + PAGE_SIZE, rwx, nullptr, 0, 0) != 0 ||
        vm_map(p->vm, top - USER_STACK_MAX, top, PROT_READ | PROT_WRITE,
               nullptr, 0, 0) != 0 ||
        vm_write(p->vm, base, code, len) != 0 ||
        !proc_thread(p, user_start, e)) {
        kfree(e);
        proc_free(p);
        return nullptr;
    }
    p->vm->brk_base = p->vm->brk = base + PAGE_SIZE;
    *e                           = (struct user_entry){.rip = base, .rsp = top};

    uint64_t flags = spin_lock_irqsave(&sched_lock);
    proc_start(p);
    spin_unlock_irqrestore(&sched_lock, flags);
    return p;
}

void proc_exit(int status) {
    struct proc *p = curthread->proc;
    if (!p) {
        thread_exit(nullptr);
    }
    vm_clear(p->vm);
    p->status = status;
    p->state  = PROC_ZOMBIE;
    thread_exit(nullptr);
}

int proc_join(struct proc *p) {
    while (p->state != PROC_ZOMBIE) {
        thread_yield();
    }
    int status = p->status;
    proc_free(p);
    return status;
}
