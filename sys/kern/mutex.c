#include <stdint.h>

#include <mutex.h>
#include <printf.h>
#include <sched.h>
#include <spinlock.h>
#include <thread.h>
#include <waitq.h>

#define PI_MAX_DEPTH 16

void mutex_init(struct mutex *m) {
    m->word      = 0;
    m->held_next = nullptr;
    m->listed    = 0;
    waitq_init(&m->waiters);
}

static struct thread *owner_of(const struct mutex *m) {
    return (struct thread *)(__atomic_load_n(&m->word, __ATOMIC_RELAXED) &
                             ~MUTEX_WAITERS);
}

static void list_on(struct mutex *m, struct thread *owner) {
    if (!m->listed) {
        m->held_next = owner->held;
        owner->held  = m;
        m->listed    = 1;
    }
}

static int inherited_priority(struct thread *t) {
    int prio = t->base_priority;
    for (struct mutex *m = t->held; m; m = m->held_next) {
        for (struct thread *w = m->waiters.head; w; w = w->next) {
            if (w->priority > prio) {
                prio = w->priority;
            }
        }
    }
    return prio;
}

void mutex_pi_update(struct thread *t) {
    for (int depth = 0; t && depth < PI_MAX_DEPTH; depth++) {
        int prio = inherited_priority(t);
        if (prio == t->priority) {
            return;
        }
        sched_set_priority(t, prio);
        t = t->blocked_on ? owner_of(t->blocked_on) : nullptr;
    }
}

static void pi_boost(struct mutex *m, int prio) {
    for (int depth = 0; m && owner_of(m) && depth < PI_MAX_DEPTH; depth++) {
        struct thread *owner = owner_of(m);
        if (owner->priority >= prio) {
            return;
        }
        sched_set_priority(owner, prio);
        m = owner->blocked_on;
    }
}

static void held_remove(struct thread *t, struct mutex *m) {
    for (struct mutex **pp = &t->held; *pp; pp = &(*pp)->held_next) {
        if (*pp == m) {
            *pp = m->held_next;
            break;
        }
    }
    m->held_next = nullptr;
    m->listed    = 0;
}

static void acquire(struct mutex *m) {
    struct thread *self = curthread;
    while (1) {
        uintptr_t w = __atomic_load_n(&m->word, __ATOMIC_RELAXED);
        if (!w) {
            uintptr_t mine =
                (uintptr_t)self | (m->waiters.head ? MUTEX_WAITERS : 0);
            if (__atomic_compare_exchange_n(&m->word, &w, mine, 0,
                                            __ATOMIC_ACQUIRE,
                                            __ATOMIC_RELAXED)) {
                break;
            }
            continue;
        }

        if (!(w & MUTEX_WAITERS) &&
            !__atomic_compare_exchange_n(&m->word, &w, w | MUTEX_WAITERS, 0,
                                         __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
            continue;
        }
        list_on(m, (struct thread *)(w & ~MUTEX_WAITERS));
        self->blocked_on = m;
        pi_boost(m, self->priority);
        waitq_sleep(&m->waiters);
    }
    self->blocked_on = nullptr;
    if (m->waiters.head) {
        list_on(m, self);
    }
    mutex_pi_update(self);
}

static void release(struct mutex *m) {
    if (m->listed) {
        held_remove(curthread, m);
    }
    __atomic_store_n(&m->word, 0, __ATOMIC_RELEASE);
    waitq_wakeup_one(&m->waiters);
    mutex_pi_update(curthread);
}

static int check_owner(struct mutex *m, const char *op) {
    if (owner_of(m) == curthread) {
        return 1;
    }
    printf("mutex: thread %d (%s) %s a mutex it does not hold\n",
           curthread->tid, curthread->name, op);
    return 0;
}

void mutex_lock(struct mutex *m) {
    uintptr_t free = 0;
    if (__atomic_compare_exchange_n(&m->word, &free, (uintptr_t)curthread, 0,
                                    __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        return;
    }
    uint64_t flags = spin_lock_irqsave(&sched_lock);

    if (owner_of(m) == curthread) {
        printf("mutex: thread %d (%s) relocked a mutex it already holds\n",
               curthread->tid, curthread->name);
    } else {
        acquire(m);
    }

    spin_unlock_irqrestore(&sched_lock, flags);
}

int mutex_trylock(struct mutex *m) {
    uintptr_t free = 0;
    return __atomic_compare_exchange_n(&m->word, &free, (uintptr_t)curthread, 0,
                                       __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)
               ? 0
               : -1;
}

void mutex_unlock(struct mutex *m) {
    uintptr_t mine = (uintptr_t)curthread;
    if (__atomic_compare_exchange_n(&m->word, &mine, 0, 0, __ATOMIC_RELEASE,
                                    __ATOMIC_RELAXED)) {
        return;
    }
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    if (check_owner(m, "released")) {
        release(m);
    }
    spin_unlock_irqrestore(&sched_lock, flags);

    sched_preempt();
}

void cond_init(struct cond *c) { waitq_init(&c->waiters); }

void cond_wait(struct cond *c, struct mutex *m) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);

    if (check_owner(m, "waited on a cond with")) {
        release(m);
        waitq_sleep(&c->waiters);
        acquire(m);
    }

    spin_unlock_irqrestore(&sched_lock, flags);
}

void cond_signal(struct cond *c) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    waitq_wakeup_one(&c->waiters);
    spin_unlock_irqrestore(&sched_lock, flags);

    sched_preempt();
}

void cond_broadcast(struct cond *c) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    waitq_wakeup_all(&c->waiters);
    spin_unlock_irqrestore(&sched_lock, flags);

    sched_preempt();
}
