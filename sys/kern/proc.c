#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <exec.h>
#include <kmalloc.h>
#include <memset.h>
#include <printf.h>
#include <proc.h>
#include <sched.h>
#include <spinlock.h>
#include <thread.h>
#include <vfs.h>
#include <vm.h>

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

static int load_program(struct vmspace *vm, const char *path,
                        struct vnode *given, const struct exec_args *a,
                        struct user_entry *e) {
    struct vnode *file = given ? vnode_ref(given) : nullptr;
    int err = given ? vfs_exec_check(given) : vfs_exec_open(path, &file);
    if (err) {
        if (given) {
            vnode_put(file);
        }
        return err;
    }
    struct exec_image img;
    char             *interp = kmalloc(EXEC_INTERP_MAX);
    err =
        interp ? exec_load(vm, file, EXEC_PIE_BASE, &img, interp, 1) : -ENOMEM;
    uintptr_t start = img.entry;
    img.interp_base = 0;
    if (!err && interp[0]) {
        struct vnode     *ld;
        struct exec_image li;
        err = vfs_exec_open(interp, &ld);
        if (!err) {
            err = exec_load(vm, ld, EXEC_INTERP_BASE, &li, nullptr, 0);
            vnode_put(ld);
        }
        start           = li.entry;
        img.interp_base = li.base;
    }
    kfree(interp);
    if (!err) {
        err    = exec_setup_stack(vm, a, &img, path, &e->rsp);
        e->rip = start;
    }
    vnode_put(file);
    return err;
}

struct proc *proc_spawn(const char *path, const char *const *argv,
                        const char *const *envp) {
    struct proc *p = proc_alloc(path);
    if (!p) {
        return nullptr;
    }

    struct exec_args   a;
    struct user_entry *e = kmalloc(sizeof(*e));
    int err              = e ? exec_args_from_kernel(&a, argv, envp) : -ENOMEM;
    if (!err) {
        err = load_program(p->vm, path, nullptr, &a, e);
        exec_args_free(&a);
    }

    if (err || !proc_thread(p, user_start, e)) {
        kfree(e);
        proc_free(p);
        return nullptr;
    }

    uint64_t flags = spin_lock_irqsave(&sched_lock);
    proc_start(p);
    spin_unlock_irqrestore(&sched_lock, flags);
    return p;
}
