#include <stddef.h>
#include <stdint.h>

#include <kmalloc.h>
#include <memset.h>
#include <panic.h>
#include <printf.h>
#include <sched.h>
#include <spinlock.h>
#include <thread.h>

#include "irq.h"
#include "switch.h"

static struct spinlock zombie_lock = {.name = "zombie"};

static struct thread *zombies;
static tid_t          next_tid = 1;

static void set_name(struct thread *t, const char *name) {
    size_t i = 0;
    for (; name[i] != '\0' && i < THREAD_NAME_MAX - 1; i++) {
        t->name[i] = name[i];
    }
    t->name[i] = '\0';
}

static void thread_start() {
    sched_unlock_new_thread();

    struct thread *t = curthread;
    thread_exit(t->entry(t->arg));
}

struct thread *thread_alloc(const char *name, thread_entry_t entry, void *arg,
                            int policy, int priority) {
    struct thread *t = kmalloc(sizeof(*t));
    if (!t) {
        return nullptr;
    }
    memset(t, 0, sizeof(*t));

    uint64_t idflags = spin_lock_irqsave(&zombie_lock);
    t->tid           = next_tid++;
    spin_unlock_irqrestore(&zombie_lock, idflags);

    t->state    = THREAD_BLOCKED;
    t->policy   = policy;
    t->priority = priority;
    t->slice    = SCHED_QUANTUM;
    t->entry    = entry;
    t->arg      = arg;
    set_name(t, name);

    if (!entry) {
        return t;
    }

    t->stack_size = THREAD_STACK_SIZE;
    t->stack      = kmalloc(t->stack_size);
    if (!t->stack) {
        kfree(t);
        return nullptr;
    }

    t->rsp = context_setup((uint8_t *)t->stack + t->stack_size, thread_start);
    return t;
}

struct thread *thread_create(const char *name, thread_entry_t entry, void *arg,
                             int policy, int priority) {
    if (priority < sched_get_priority_min(policy) ||
        priority > sched_get_priority_max(policy)) {
        return nullptr;
    }

    struct thread *t = thread_alloc(name, entry, arg, policy, priority);
    if (!t) {
        return nullptr;
    }

    uint64_t flags = spin_lock_irqsave(&sched_lock);
    sched_enqueue(t);
    spin_unlock_irqrestore(&sched_lock, flags);

    return t;
}

void thread_yield() {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    schedule();
    spin_unlock_irqrestore(&sched_lock, flags);
}

static void make_reapable(struct thread *t) {
    uint64_t flags = spin_lock_irqsave(&zombie_lock);
    t->next        = zombies;
    zombies        = t;
    spin_unlock_irqrestore(&zombie_lock, flags);
}

void thread_exit(void *retval) {
    spin_lock_irqsave(&sched_lock);
    thread_exit_locked(retval);
}

void thread_exit_locked(void *retval) {
    curthread->retval = retval;
    curthread->state  = THREAD_ZOMBIE;
    make_reapable(curthread);

    schedule();

    panic("thread: exited thread was rescheduled");
}

void thread_reap() {
    while (1) {
        uint64_t       flags = spin_lock_irqsave(&zombie_lock);
        struct thread *t     = zombies;
        if (t) {
            zombies = t->next;
        }
        spin_unlock_irqrestore(&zombie_lock, flags);

        if (!t) {
            return;
        }

        kfree(t->stack);
        kfree(t);
    }
}
