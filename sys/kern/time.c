#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <ktime.h>
#include <printf.h>
#include <sched.h>
#include <spinlock.h>
#include <thread.h>
#include <waitq.h>

static uint64_t boot_cycles, hz;
static int64_t  real_offset;

uint64_t time_mono() {
    if (!hz) {
        return 0;
    }
    uint64_t d = machdep_cycles() - boot_cycles;
    return d / hz * NSEC + d % hz * NSEC / hz;
}

int64_t time_real() { return (int64_t)time_mono() + real_offset; }

void time_init() {
    hz          = machdep_cycles_hz();
    boot_cycles = machdep_cycles();
    real_offset = (int64_t)rtc_read() * NSEC - (int64_t)time_mono();
    printf("time: %ld seconds since the epoch\n", (long)(time_real() / NSEC));
}

static uint64_t thread_cpu(struct thread *t) {
    return t->cpu_ns +
           (t->state == THREAD_RUNNING ? time_mono() - t->start_ns : 0);
}

void time_switch(struct thread *prev, struct thread *next) {
    uint64_t now = time_mono();
    prev->cpu_ns += now - prev->start_ns;
    next->start_ns = now;
}

uint64_t time_thread_cpu(struct thread *t) { return thread_cpu(t); }

enum { KT_WAKE, KT_SIGNAL };

struct ktimer {
    uint64_t       when;
    int            armed, kind;
    struct thread *thread;
    struct ktimer *next;
};

static struct ktimer *armed;

static void disarm(struct ktimer *t) {
    if (!t->armed) {
        return;
    }
    for (struct ktimer **pp = &armed; *pp; pp = &(*pp)->next) {
        if (*pp == t) {
            *pp = t->next;
            break;
        }
    }
    t->armed = 0;
}

static void arm(struct ktimer *t, uint64_t when) {
    disarm(t);
    t->when            = when;
    struct ktimer **pp = &armed;
    while (*pp && (*pp)->when <= when) {
        pp = &(*pp)->next;
    }
    t->next  = *pp;
    *pp      = t;
    t->armed = 1;
}

static void fire(struct ktimer *t) {
    if (t->kind == KT_WAKE) {
        struct thread *th = t->thread;
        th->timed_out     = 1;
        if (th->state == THREAD_BLOCKED && th->sleep_wq) {
            waitq_remove(th->sleep_wq, th);
            th->sleep_wq = nullptr;
            sched_enqueue(th);
        }
        return;
    }
}

void time_tick(int user) {
    (void)user;
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    uint64_t now   = time_mono();
    while (armed && armed->when <= now) {
        struct ktimer *t = armed;
        armed            = t->next;
        t->armed         = 0;
        fire(t);
    }
    spin_unlock_irqrestore(&sched_lock, flags);
}

int sleep_until(struct waitq *wq, uint64_t deadline) {
    struct thread *t  = curthread;
    struct ktimer  kt = {.kind = KT_WAKE, .thread = t};
    struct waitq   own;
    if (!wq) {
        waitq_init(&own);
        wq = &own;
    }
    if (deadline <= time_mono()) {
        return -ETIMEDOUT;
    }
    t->timed_out = 0;
    arm(&kt, deadline);
    int r = waitq_sleep_intr(wq);
    disarm(&kt);
    return t->timed_out ? -ETIMEDOUT : r;
}
