#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <exec.h>
#include <fd.h>
#include <kmalloc.h>
#include <memset.h>
#include <proc.h>
#include <sched.h>
#include <spinlock.h>
#include <thread.h>
#include <vfs.h>
#include <vm.h>
#include <waitq.h>

#include "pmap.h"
#include "user.h"

struct proc         proc0    = {.name = "kernel"};
static pid_t        next_pid = 1;
static struct proc *initproc;
struct proc        *proc_list;

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
    waitq_init(&p->child_exit);
    spin_lock_init(&p->files_lock, "files");

    p->vm = vm_create();
    if (!p->vm) {
        kfree(p);
        return nullptr;
    }
    return p;
}

static void drop_cwd(struct proc *p) {
    if (p->cwd) {
        vnode_put(p->cwd);
        p->cwd = nullptr;
    }
}

static void proc_free(struct proc *p) {
    fd_close_all(p);
    drop_cwd(p);
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

static void proc_start(struct proc *p, struct proc *parent) {
    p->pid = next_pid++;
    p->cwd = parent->cwd ? vnode_ref(parent->cwd) : nullptr;
    if (p->pid == 1) {
        initproc = p;
    }
    p->all_next      = proc_list;
    proc_list        = p;
    p->parent        = parent;
    p->sibling       = parent->children;
    parent->children = p;
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

    struct file *console = err ? nullptr : vfs_console();
    if (!console || !proc_thread(p, user_start, e)) {
        if (console) {
            file_unref(console);
        }
        kfree(e);
        proc_free(p);
        return nullptr;
    }
    p->fds[0] = console;
    p->fds[1] = file_ref(console);
    p->fds[2] = file_ref(console);

    uint64_t flags = spin_lock_irqsave(&sched_lock);
    proc_start(p, curproc());
    spin_unlock_irqrestore(&sched_lock, flags);
    return p;
}

int proc_exec(const char *path, struct vnode *given,
              const struct exec_args *a) {
    struct proc *p = curproc();
    if (!p) {
        return -EINVAL;
    }

    struct vmspace *vm = vm_create();
    if (!vm) {
        return -ENOMEM;
    }
    struct user_entry e;
    int               err = load_program(vm, path, given, a, &e);
    if (err) {
        vm_destroy(vm);
        return err;
    }

    uint64_t        flags = spin_lock_irqsave(&sched_lock);
    struct vmspace *old   = p->vm;
    p->vm                 = vm;
    curthread->pmap       = vm->pmap;
    pmap_activate(vm->pmap);
    spin_unlock_irqrestore(&sched_lock, flags);

    vm_destroy(old);
    set_name(p, path);
    fd_close_on_exec(p);
    context_exec(e.rip, e.rsp);
    return 0;
}

static void *fork_placeholder(void *arg) { return arg; }

pid_t proc_fork() {
    struct proc *parent = curthread->proc;
    if (!parent) {
        return -EINVAL;
    }

    struct proc *child = proc_alloc(parent->name);
    if (!child) {
        return -ENOMEM;
    }
    if (vm_copy(child->vm, parent->vm) != 0 ||
        !proc_thread(child, fork_placeholder, nullptr)) {
        proc_free(child);
        return -ENOMEM;
    }
    context_setup_fork(child->threads);
    sched_setscheduler(child->threads, curthread->policy,
                       curthread->base_priority);
    fd_fork(child, parent);

    uint64_t flags = spin_lock_irqsave(&sched_lock);
    proc_start(child, parent);
    pid_t pid = child->pid;
    spin_unlock_irqrestore(&sched_lock, flags);
    return pid;
}

void proc_exit(int status) {
    struct proc *p = curthread->proc;
    if (!p) {
        thread_exit(nullptr);
    }
    proc_thread_exit(status);
}

[[noreturn]] static void teardown(struct proc *p, int status);

void proc_thread_exit(int status) {
    struct thread *t = curthread;
    struct proc   *p = t->proc;
    fd_return_borrowed();

    uint64_t flags = spin_lock_irqsave(&sched_lock);
    for (struct thread **tp = &p->threads; *tp; tp = &(*tp)->proc_next) {
        if (*tp == t) {
            *tp = t->proc_next;
            break;
        }
    }
    if (--p->nthreads > 0) {
        thread_exit_locked(nullptr);
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    teardown(p, status);
}

[[noreturn]] static void teardown(struct proc *p, int status) {
    fd_close_all(p);
    drop_cwd(p);
    vm_clear(p->vm);

    spin_lock_irqsave(&sched_lock);

    struct proc *heir = (initproc && initproc != p) ? initproc : &proc0;
    while (p->children) {
        struct proc *c = p->children;
        p->children    = c->sibling;
        c->parent      = heir;
        c->sibling     = heir->children;
        heir->children = c;
        if (c->state == PROC_ZOMBIE) {
            waitq_wakeup_all(&heir->child_exit);
        }
    }

    p->state  = PROC_ZOMBIE;
    p->status = status;

    struct proc *parent = p->parent;
    waitq_wakeup_all(&parent->child_exit);

    thread_exit_locked(nullptr);
}

static int wanted(struct proc *self, struct proc *c, pid_t pid) {
    (void)self;
    return pid > 0 ? c->pid == pid : 1;
}

pid_t proc_wait(pid_t pid, int *status, int options, struct siginfo *info,
                void *rusage) {
    struct proc *self  = curproc();
    uint64_t     flags = spin_lock_irqsave(&sched_lock);
    (void)info;
    (void)rusage;

    while (1) {
        int found = 0;
        for (struct proc **pp = &self->children; *pp; pp = &(*pp)->sibling) {
            struct proc *c = *pp;
            if (!wanted(self, c, pid)) {
                continue;
            }
            found = 1;
            if (c->state == PROC_ALIVE) {
                continue;
            }
            if (!(options & WEXITED)) {
                continue;
            }
            pid_t cpid = c->pid;
            if (status) {
                *status = c->status;
            }
            if (options & WNOWAIT) {
                spin_unlock_irqrestore(&sched_lock, flags);
                return cpid;
            }

            *pp = c->sibling;
            for (struct proc **lp = &proc_list; *lp; lp = &(*lp)->all_next) {
                if (*lp == c) {
                    *lp = c->all_next;
                    break;
                }
            }
            spin_unlock_irqrestore(&sched_lock, flags);
            proc_free(c);
            return cpid;
        }

        if (!found || (options & WNOHANG)) {
            spin_unlock_irqrestore(&sched_lock, flags);
            return !found ? -ECHILD : 0;
        }
        waitq_sleep(&self->child_exit);
    }
}
