#include <stdint.h>

#include <cpu.h>
#include <ktime.h>
#include <mutex.h>
#include <printf.h>
#include <sched.h>
#include <spinlock.h>
#include <thread.h>

#include "irq.h"
#include "pmap.h"
#include "switch.h"
#include "user.h"

struct runqueue {
    struct thread *head;
    struct thread *tail;
};

struct spinlock sched_lock;

static struct runqueue runq[SCHED_PRIO_LEVELS];
static uint64_t        runq_bitmap[(SCHED_PRIO_LEVELS + 63) / 64];

int sched_get_priority_min(int policy) { return policy == SCHED_OTHER ? 0 : 1; }

int sched_get_priority_max(int policy) {
    return policy == SCHED_OTHER ? 0 : SCHED_PRIO_MAX;
}

static void kick(struct cpu *c) { c->need_resched = 1; }

static int running_priority(const struct cpu *c) {
    return !c->thread || c->thread == c->idle ? -1 : c->thread->priority;
}

static void runq_unlink(struct thread *t) {
    struct runqueue *rq = &runq[t->priority];

    if (t->prev) {
        t->prev->next = t->next;
    } else {
        rq->head = t->next;
    }
    if (t->next) {
        t->next->prev = t->prev;
    } else {
        rq->tail = t->prev;
    }
    if (!rq->head) {
        runq_bitmap[t->priority / 64] &= ~(1ull << (t->priority % 64));
    }

    t->next = nullptr;
    t->prev = nullptr;
}

static int runq_highest_priority() {
    for (int w = (SCHED_PRIO_LEVELS + 63) / 64 - 1; w >= 0; w--) {
        if (runq_bitmap[w]) {
            return w * 64 + (63 - __builtin_clzll(runq_bitmap[w]));
        }
    }
    return -1;
}

void sched_set_priority(struct thread *t, int priority) {
    if (t->priority == priority) {
        return;
    }

    if (t->state == THREAD_READY && t != curcpu()->idle) {
        runq_unlink(t);
        t->priority = priority;
        sched_enqueue(t);
        return;
    }

    t->priority = priority;
    if (t->state == THREAD_RUNNING && runq_highest_priority() > priority) {
        for (int i = 0; i < ncpu; i++) {
            if (cpus[i].thread == t) {
                kick(&cpus[i]);
            }
        }
    }
}

int sched_setscheduler(struct thread *t, int policy, int priority) {
    if (policy != SCHED_OTHER && policy != SCHED_FIFO && policy != SCHED_RR) {
        return -1;
    }

    if (priority < sched_get_priority_min(policy) ||
        priority > sched_get_priority_max(policy)) {
        return -1;
    }

    uint64_t flags   = spin_lock_irqsave(&sched_lock);
    t->policy        = policy;
    t->base_priority = priority;
    mutex_pi_update(t);
    spin_unlock_irqrestore(&sched_lock, flags);

    sched_preempt();
    return 0;
}

static void runq_append(struct thread *t) {
    struct runqueue *rq = &runq[t->priority];

    t->state = THREAD_READY;
    t->next  = nullptr;
    t->prev  = rq->tail;

    if (rq->tail) {
        rq->tail->next = t;
    } else {
        rq->head = t;
    }
    rq->tail = t;

    runq_bitmap[t->priority / 64] |= 1ull << (t->priority % 64);
}

void sched_enqueue(struct thread *t) {
    runq_append(t);

    struct cpu *best = nullptr;
    for (int i = -1; i < ncpu; i++) {
        struct cpu *c = i < 0 ? curcpu() : &cpus[i];
        if (c->thread && !c->need_resched &&
            (!best || running_priority(c) < running_priority(best))) {
            best = c;
        }
    }
    if (best && running_priority(best) < t->priority) {
        kick(best);
    }
}

static struct thread *runq_take_highest() {
    int prio = runq_highest_priority();
    if (prio < 0) {
        return nullptr;
    }

    struct thread *t = runq[prio].head;
    runq_unlink(t);
    return t;
}

void schedule() {
    struct cpu    *c    = curcpu();
    struct thread *prev = c->thread;

    if (prev->state == THREAD_RUNNING) {
        prev->state = THREAD_READY;
        if (prev != c->idle) {
            runq_append(prev);
        }
    }

    struct thread *next = runq_take_highest();
    if (!next) {
        next = c->idle;
    }

    c->need_resched = 0;
    next->state     = THREAD_RUNNING;
    next->slice     = SCHED_QUANTUM;

    if (next == prev) {
        return;
    }

    c->thread = next;
    if (next->stack) {
        context_set_kstack((uint64_t)next->stack + next->stack_size);
    }
    pmap_activate(next->pmap ? next->pmap : &kernel_pmap);
    context_fpu_switch(prev, next);
    time_switch(prev, next);
    context_switch(&prev->rsp, next->rsp);

    thread_reap();
}

void sched_unlock_new_thread() {
    spin_unlock(&sched_lock);
    irq_enable();
}

void sched_tick() {
    if (!curthread) {
        return;
    }

    if (curthread->policy == SCHED_FIFO) {
        return;
    }

    if (curthread->slice > 0 && --curthread->slice == 0) {
        curcpu()->need_resched = 1;
    }
}

void sched_preempt() {
    if (!curthread || !curcpu()->need_resched) {
        return;
    }

    uint64_t flags = spin_lock_irqsave(&sched_lock);
    if (curcpu()->need_resched) {
        schedule();
    }
    spin_unlock_irqrestore(&sched_lock, flags);
}

static void *idle_loop(void *arg) {
    (void)arg;

    while (1) {
        thread_reap();
        // arch depends
        __asm__ __volatile__("sti\n\thlt");
    }
}

void sched_init() {
    struct cpu *cpu = curcpu();

    spin_lock_init(&sched_lock, "sched");

    cpu->id           = 0;
    cpu->need_resched = 0;

    for (int i = 0; i < SCHED_PRIO_LEVELS; i++) {
        runq[i].head = nullptr;
        runq[i].tail = nullptr;
    }
    for (unsigned i = 0; i < sizeof(runq_bitmap) / sizeof(*runq_bitmap); i++) {
        runq_bitmap[i] = 0;
    }

    struct thread *main =
        thread_alloc("main", nullptr, nullptr, SCHED_OTHER, 0);
    main->state = THREAD_RUNNING;
    main->slice = SCHED_QUANTUM;
    cpu->thread = main;

    cpu->idle        = thread_alloc("idle", idle_loop, nullptr, SCHED_OTHER, 0);
    cpu->idle->state = THREAD_READY;

    printf("sched: initialized\n");
}
