#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <kmalloc.h>
#include <ktime.h>
#include <printf.h>
#include <proc.h>
#include <sched.h>
#include <signal.h>
#include <spinlock.h>
#include <thread.h>
#include <uaccess.h>
#include <waitq.h>

// --- Clock -------------------------------------------------------------------

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

static uint64_t proc_cpu(struct proc *p) {
    uint64_t ns = p->cpu_ns;
    for (struct thread *t = p->threads; t; t = t->proc_next) {
        ns += thread_cpu(t);
    }
    return ns;
}

uint64_t time_thread_cpu(struct thread *t) { return thread_cpu(t); }

#define CPUCLOCK_PID(c)    (~(c) >> 3)
#define CPUCLOCK_SCHED     2
#define CPUCLOCK_PERTHREAD 4

static int clock_read(long clock, int64_t *ns) {
    switch (clock) {
    case CLOCK_REALTIME:
    case CLOCK_REALTIME_COARSE:
        *ns = time_real();
        return 0;
    case CLOCK_MONOTONIC:
    case CLOCK_MONOTONIC_RAW:
    case CLOCK_MONOTONIC_COARSE:
    case CLOCK_BOOTTIME:
        *ns = (int64_t)time_mono();
        return 0;
    case CLOCK_THREAD_CPUTIME_ID:
        *ns = (int64_t)thread_cpu(curthread);
        return 0;
    }
    if (clock != CLOCK_PROCESS_CPUTIME_ID &&
        (clock >= 0 || (clock & 3) != CPUCLOCK_SCHED)) {
        return -EINVAL;
    }
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    int      err   = 0;
    if (clock == CLOCK_PROCESS_CPUTIME_ID) {
        *ns = (int64_t)proc_cpu(curproc());
    } else if (clock & CPUCLOCK_PERTHREAD) {
        struct thread *t = proc_find_thread_locked((pid_t)CPUCLOCK_PID(clock));
        err              = t ? 0 : -EINVAL;
        if (t) {
            *ns = (int64_t)thread_cpu(t);
        }
    } else {
        struct proc *p = proc_find_locked((pid_t)CPUCLOCK_PID(clock));
        err            = p ? 0 : -EINVAL;
        if (p) {
            *ns = (int64_t)proc_cpu(p);
        }
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    return err;
}

static int is_cpu_clock(long clock) {
    return clock == CLOCK_PROCESS_CPUTIME_ID ||
           clock == CLOCK_THREAD_CPUTIME_ID || clock < 0;
}

static int get_ts(const struct k_timespec *u, int64_t *ns) {
    struct k_timespec ts;
    if (copy_from_user(&ts, u, sizeof(ts)) != 0) {
        return -EFAULT;
    }
    if (ts.sec < 0 || ts.nsec < 0 || ts.nsec >= NSEC) {
        return -EINVAL;
    }
    *ns = ts.sec > INT64_MAX / NSEC - 1 ? INT64_MAX : ts.sec * NSEC + ts.nsec;
    return 0;
}

static int put_ts(struct k_timespec *u, int64_t ns) {
    struct k_timespec ts = {ns / NSEC, ns % NSEC};
    return copy_to_user(u, &ts, sizeof(ts));
}

long sys_clock_gettime(long clock, struct k_timespec *uts) {
    int64_t ns;
    int     err = clock_read(clock, &ns);
    return err ? err : put_ts(uts, ns);
}

long sys_clock_getres(long clock, struct k_timespec *uts) {
    int64_t ns;
    int     err = clock_read(clock, &ns);
    if (err || !uts) {
        return err;
    }
    int coarse =
        clock == CLOCK_REALTIME_COARSE || clock == CLOCK_MONOTONIC_COARSE;
    return put_ts(uts, coarse ? TICK_NS : 1);
}

// --- Timer -------------------------------------------------------------------

enum { KT_WAKE, KT_SIGNAL };

struct ktimer {
    uint64_t       when;
    uint64_t       interval;
    int            armed, kind;
    int            abs_real;
    int64_t        real_when;
    struct thread *thread;
    struct proc   *proc;
    int            clock, signo, code, id, notify;
    int            tid;
    uint64_t       value;
    int            pending;
    int            overrun, last_overrun;
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

static void fire(struct ktimer *t, uint64_t now) {
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
    struct proc *p = t->proc;
    if (t->notify && p->state == PROC_ALIVE) {
        if (t->pending) {
            t->overrun += t->overrun < INT32_MAX;
        } else {
            struct siginfo info = {.si_signo = t->signo, .si_code = t->code};
            struct thread *th =
                t->tid ? proc_find_thread_locked(t->tid) : nullptr;
            if (t->tid && (!th || th->proc != p)) {
            } else if (t->code == SI_TIMER) {
                info.pid   = t->id;
                info.value = t->value;
                t->pending = (th ? signal_send_thread_locked(th, &info)
                                 : signal_send_info_locked(p, &info)) == 1;
            } else {
                signal_send_info_locked(p, &info);
            }
        }
    }
    if (t->interval) {
        uint64_t next = t->when + t->interval;
        if (next <= now) {
            uint64_t missed = (now - next) / t->interval + 1;
            if (t->pending) {
                t->overrun = (int)(t->overrun + missed > INT32_MAX
                                       ? INT32_MAX
                                       : t->overrun + missed);
            }
            next += missed * t->interval;
        }
        t->abs_real = 0;
        arm(t, next);
    }
}

void time_timer_taken(struct proc *p, struct siginfo *info) {
    int id = info->pid;
    if (id >= 0 && id < TIMER_MAX && p->timers[id]) {
        struct ktimer *t = p->timers[id];
        info->uid        = (uint32_t)t->overrun;
        t->last_overrun  = t->overrun;
        t->overrun       = 0;
        t->pending       = 0;
    }
}

static void itimer_tick(struct proc *p, int which, int sig) {
    uint64_t *left = &p->itimer_left[which];
    if (!*left) {
        return;
    }
    if (*left > TICK_NS) {
        *left -= TICK_NS;
        return;
    }
    *left = p->itimer_interval[which];
    signal_send_locked(p, sig);
}

void time_tick(int user) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    uint64_t now   = time_mono();
    while (armed && armed->when <= now) {
        struct ktimer *t = armed;
        armed            = t->next;
        t->armed         = 0;
        fire(t, now);
    }
    struct proc *p = curthread ? curthread->proc : nullptr;
    if (p) {
        if (user) {
            p->utime++;
            itimer_tick(p, 1, SIGVTALRM);
        } else {
            p->stime++;
        }
        itimer_tick(p, 2, SIGPROF);
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

static uint64_t to_mono(int64_t real) {
    int64_t m = real - real_offset;
    return m < 0 ? 0 : (uint64_t)m;
}

static void clock_was_set() {
    struct ktimer *moved = nullptr;
    for (struct ktimer **pp = &armed; *pp;) {
        struct ktimer *t = *pp;
        if (t->abs_real) {
            *pp      = t->next;
            t->next  = moved;
            moved    = t;
            t->armed = 0;
            continue;
        }
        pp = &t->next;
    }
    while (moved) {
        struct ktimer *t = moved;
        moved            = t->next;
        arm(t, to_mono(t->real_when));
    }
}

long sys_clock_settime(long clock, const struct k_timespec *uts) {
    int64_t ns;
    int     err = get_ts(uts, &ns);
    if (err) {
        return err;
    }
    if (clock != CLOCK_REALTIME) {
        return -EINVAL;
    }
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    real_offset    = ns - (int64_t)time_mono();
    clock_was_set();
    spin_unlock_irqrestore(&sched_lock, flags);
    return 0;
}

// --- Sleep -------------------------------------------------------------------

static long sleep_for(int64_t ns, struct k_timespec *urem) {
    uint64_t deadline = time_mono() + (uint64_t)ns;
    uint64_t flags    = spin_lock_irqsave(&sched_lock);
    int      r;
    while ((r = sleep_until(nullptr, deadline)) == 0) {
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    if (r == -EINTR) {
        uint64_t now = time_mono();
        if (urem &&
            put_ts(urem, now < deadline ? (int64_t)(deadline - now) : 0) != 0) {
            return -EFAULT;
        }
        return -EINTR;
    }
    return 0;
}

long sys_nanosleep(const struct k_timespec *ureq, struct k_timespec *urem) {
    int64_t ns;
    int     err = get_ts(ureq, &ns);
    return err ? err : sleep_for(ns, urem);
}

long sys_clock_nanosleep(long clock, long flags, const struct k_timespec *ureq,
                         struct k_timespec *urem) {
    int64_t ns, now;
    int     err = clock_read(clock, &now);
    if (err) {
        return err;
    }
    if (is_cpu_clock(clock)) {
        return clock == CLOCK_THREAD_CPUTIME_ID ? -EINVAL : -EOPNOTSUPP;
    }
    if ((err = get_ts(ureq, &ns)) != 0) {
        return err;
    }
    if (!(flags & TIMER_ABSTIME)) {
        return sleep_for(ns, urem);
    }

    struct ktimer kt = {.kind      = KT_WAKE,
                        .thread    = curthread,
                        .abs_real  = clock == CLOCK_REALTIME,
                        .real_when = ns};
    uint64_t      lf = spin_lock_irqsave(&sched_lock);
    struct waitq  wq;
    waitq_init(&wq);
    int r = 0;
    while (!r) {
        uint64_t when = kt.abs_real ? to_mono(ns) : (uint64_t)ns;
        if (when <= time_mono()) {
            break;
        }
        curthread->timed_out = 0;
        arm(&kt, when);
        r = waitq_sleep_intr(&wq);
        disarm(&kt);
        if (curthread->timed_out) {
            r = 0;
        }
    }
    spin_unlock_irqrestore(&sched_lock, lf);
    return r == -EINTR ? -EINTR : 0;
}

// --- Itimers -----------------------------------------------------------------

#define ITIMER_REAL    0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF    2

struct k_timeval {
    int64_t sec, usec;
};

struct k_itimerval {
    struct k_timeval interval, value;
};

static uint64_t tv_ns(struct k_timeval tv) {
    return (uint64_t)tv.sec * NSEC + (uint64_t)tv.usec * 1000;
}

static struct k_timeval ns_tv(uint64_t ns) {
    uint64_t us = (ns + 999) / 1000;
    return (struct k_timeval){(int64_t)(us / 1000000), (int64_t)(us % 1000000)};
}

static void itimer_get(struct proc *p, int which, struct k_itimerval *out) {
    uint64_t left = 0, interval = 0;
    if (which == ITIMER_REAL) {
        struct ktimer *t = p->itimer_real;
        if (t && t->armed) {
            uint64_t now = time_mono();
            left         = t->when > now ? t->when - now : 1;
            interval     = t->interval;
        }
    } else {
        left     = p->itimer_left[which];
        interval = p->itimer_interval[which];
    }
    out->value    = ns_tv(left);
    out->interval = ns_tv(interval);
}

long sys_getitimer(long which, void *ucur) {
    if (which < ITIMER_REAL || which > ITIMER_PROF) {
        return -EINVAL;
    }
    struct k_itimerval cur;
    uint64_t           flags = spin_lock_irqsave(&sched_lock);
    itimer_get(curproc(), (int)which, &cur);
    spin_unlock_irqrestore(&sched_lock, flags);
    return copy_to_user(ucur, &cur, sizeof(cur));
}

long sys_setitimer(long which, const void *unew, void *uold) {
    struct k_itimerval nv = {0}, old;
    if (which < ITIMER_REAL || which > ITIMER_PROF) {
        return -EINVAL;
    }
    if (unew && copy_from_user(&nv, unew, sizeof(nv)) != 0) {
        return -EFAULT;
    }
    if (nv.value.usec < 0 || nv.value.usec >= 1000000 || nv.value.sec < 0 ||
        nv.interval.usec < 0 || nv.interval.usec >= 1000000 ||
        nv.interval.sec < 0) {
        return -EINVAL;
    }
    struct proc   *p = curproc();
    struct ktimer *t = nullptr;
    if (which == ITIMER_REAL && !p->itimer_real && !(t = kmalloc(sizeof(*t)))) {
        return -ENOMEM;
    }
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    if (t) {
        *t             = (struct ktimer){.kind   = KT_SIGNAL,
                                         .proc   = p,
                                         .signo  = SIGALRM,
                                         .code   = SI_KERNEL,
                                         .notify = 1};
        p->itimer_real = t;
    }
    itimer_get(p, (int)which, &old);
    if (unew) {
        uint64_t value    = tv_ns(nv.value);
        uint64_t interval = tv_ns(nv.interval);
        if (which == ITIMER_REAL) {
            disarm(p->itimer_real);
            p->itimer_real->interval = interval;
            if (value) {
                arm(p->itimer_real, time_mono() + value);
            }
        } else {
            p->itimer_left[which]     = value;
            p->itimer_interval[which] = value ? interval : 0;
        }
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    return uold ? copy_to_user(uold, &old, sizeof(old)) : 0;
}

// --- POSIX timers ------------------------------------------------------------

#define SIGEV_SIGNAL    0
#define SIGEV_NONE      1
#define SIGEV_THREAD_ID 4

struct k_sigevent {
    uint64_t value;
    int32_t  signo, notify;
    int32_t  pad[12];
};

struct k_itimerspec {
    struct k_timespec interval, value;
};

long sys_timer_create(long clock, const void *usev, int *uid) {
    int64_t now;
    int     err = clock_read(clock, &now);
    if (err) {
        return err;
    }
    if (is_cpu_clock(clock)) {
        // no cpu timers yet
        return -EOPNOTSUPP;
    }

    struct k_sigevent sev = {.signo = SIGALRM, .notify = SIGEV_SIGNAL};
    if (usev && copy_from_user(&sev, usev, sizeof(sev)) != 0) {
        return -EFAULT;
    }
    int signals = sev.notify == SIGEV_SIGNAL || sev.notify == SIGEV_THREAD_ID;
    if ((!signals && sev.notify != SIGEV_NONE) ||
        (signals && (sev.signo <= 0 || sev.signo >= NSIG))) {
        return -EINVAL;
    }
    if (sev.notify == SIGEV_THREAD_ID) {
        uint64_t       f  = spin_lock_irqsave(&sched_lock);
        struct thread *th = proc_find_thread_locked(sev.pad[0]);
        int            ok = th && th->proc == curproc();
        spin_unlock_irqrestore(&sched_lock, f);
        if (!ok) {
            return -EINVAL;
        }
    }
    struct ktimer *t = kmalloc(sizeof(*t));
    if (!t) {
        return -ENOMEM;
    }
    struct proc *p     = curproc();
    uint64_t     flags = spin_lock_irqsave(&sched_lock);
    int          id    = 0;
    while (id < TIMER_MAX && p->timers[id]) {
        id++;
    }
    if (id == TIMER_MAX) {
        spin_unlock_irqrestore(&sched_lock, flags);
        kfree(t);
        return -EAGAIN;
    }
    *t = (struct ktimer){.kind   = KT_SIGNAL,
                         .proc   = p,
                         .clock  = (int)clock,
                         .signo  = sev.signo,
                         .code   = SI_TIMER,
                         .id     = id,
                         .notify = signals,
                         .tid = sev.notify == SIGEV_THREAD_ID ? sev.pad[0] : 0,
                         .value = usev ? sev.value : (uint64_t)id};
    p->timers[id] = t;
    spin_unlock_irqrestore(&sched_lock, flags);
    if (copy_to_user(uid, &id, sizeof(id)) != 0) {
        sys_timer_delete(id);
        return -EFAULT;
    }
    return 0;
}

static struct ktimer *timer_of(long id) {
    struct proc *p = curproc();
    return id >= 0 && id < TIMER_MAX ? p->timers[id] : nullptr;
}

static void timer_get(struct ktimer *t, struct k_itimerspec *out) {
    uint64_t now  = time_mono();
    int64_t  left = t->armed ? (int64_t)(t->when > now ? t->when - now : 1) : 0;
    *out          = (struct k_itimerspec){
        {(int64_t)t->interval / NSEC, (int64_t)t->interval % NSEC},
        {left / NSEC, left % NSEC}};
}

long sys_timer_settime(long id, long tflags, const void *unew, void *uold) {
    struct k_itimerspec nv, old;
    if (copy_from_user(&nv, unew, sizeof(nv)) != 0) {
        return -EFAULT;
    }
    if (nv.value.nsec < 0 || nv.value.nsec >= NSEC || nv.value.sec < 0 ||
        nv.interval.nsec < 0 || nv.interval.nsec >= NSEC ||
        nv.interval.sec < 0) {
        return -EINVAL;
    }
    uint64_t       flags = spin_lock_irqsave(&sched_lock);
    struct ktimer *t     = timer_of(id);
    if (!t) {
        spin_unlock_irqrestore(&sched_lock, flags);
        return -EINVAL;
    }
    timer_get(t, &old);
    disarm(t);
    int64_t value = nv.value.sec * NSEC + nv.value.nsec;
    t->interval   = (uint64_t)(nv.interval.sec * NSEC + nv.interval.nsec);
    t->abs_real   = 0;
    if (value) {
        uint64_t when;
        if (!(tflags & TIMER_ABSTIME)) {
            when = time_mono() + (uint64_t)value;
        } else if (t->clock == CLOCK_REALTIME) {
            t->abs_real  = 1;
            t->real_when = value;
            when         = to_mono(value);
        } else {
            when = (uint64_t)value;
        }
        arm(t, when);
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    return uold ? copy_to_user(uold, &old, sizeof(old)) : 0;
}

long sys_timer_gettime(long id, void *ucur) {
    struct k_itimerspec cur;
    uint64_t            flags = spin_lock_irqsave(&sched_lock);
    struct ktimer      *t     = timer_of(id);
    if (t) {
        timer_get(t, &cur);
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    return !t ? -EINVAL : copy_to_user(ucur, &cur, sizeof(cur));
}

long sys_timer_getoverrun(long id) {
    uint64_t       flags = spin_lock_irqsave(&sched_lock);
    struct ktimer *t     = timer_of(id);
    long           r     = t ? t->last_overrun : -EINVAL;
    spin_unlock_irqrestore(&sched_lock, flags);
    return r;
}

long sys_timer_delete(long id) {
    struct proc   *p     = curproc();
    uint64_t       flags = spin_lock_irqsave(&sched_lock);
    struct ktimer *t     = timer_of(id);
    if (t) {
        disarm(t);
        p->timers[id] = nullptr;
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    kfree(t);
    return t ? 0 : -EINVAL;
}

// --- Process lifetimes -------------------------------------------------------

static void drop_timers(struct proc *p) {
    for (int i = 0; i < TIMER_MAX; i++) {
        if (p->timers[i]) {
            disarm(p->timers[i]);
            kfree(p->timers[i]);
            p->timers[i] = nullptr;
        }
    }
}

void time_fork(struct proc *child) {
    child->itimer_real = nullptr;
    for (int i = 0; i < 3; i++) {
        child->itimer_left[i] = child->itimer_interval[i] = 0;
    }
    for (int i = 0; i < TIMER_MAX; i++) {
        child->timers[i] = nullptr;
    }
    child->utime = child->stime = child->cutime = child->cstime = 0;
}

void time_exec(struct proc *p) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    drop_timers(p);
    spin_unlock_irqrestore(&sched_lock, flags);
}

void time_exit(struct proc *p) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    drop_timers(p);
    if (p->itimer_real) {
        disarm(p->itimer_real);
        kfree(p->itimer_real);
        p->itimer_real = nullptr;
    }
    p->itimer_left[1] = p->itimer_left[2] = 0;
    spin_unlock_irqrestore(&sched_lock, flags);
}

void time_reap(struct proc *parent, struct proc *child) {
    parent->cutime += child->utime + child->cutime;
    parent->cstime += child->stime + child->cstime;
}

// --- Accounting --------------------------------------------------------------

#define CLK_TCK 100

long sys_times(void *utms) {
    struct proc *p      = curproc();
    int64_t      tms[4] = {(int64_t)p->utime * CLK_TCK / TICK_HZ,
                           (int64_t)p->stime * CLK_TCK / TICK_HZ,
                           (int64_t)p->cutime * CLK_TCK / TICK_HZ,
                           (int64_t)p->cstime * CLK_TCK / TICK_HZ};
    if (utms && copy_to_user(utms, tms, sizeof(tms)) != 0) {
        return -EFAULT;
    }
    return (long)(time_mono() / (NSEC / CLK_TCK));
}

struct k_rusage {
    struct k_timeval utime, stime;
    int64_t          rest[14];
};

static struct k_timeval ticks_tv(uint64_t ticks) {
    uint64_t us = ticks * (1000000 / TICK_HZ);
    return (struct k_timeval){(int64_t)(us / 1000000), (int64_t)(us % 1000000)};
}

void time_rusage(struct proc *p, int which, void *out) {
    uint64_t u  = which != 1 ? p->utime : 0;
    uint64_t st = which != 1 ? p->stime : 0;
    if (which) {
        u += p->cutime;
        st += p->cstime;
    }
    *(struct k_rusage *)out =
        (struct k_rusage){.utime = ticks_tv(u), .stime = ticks_tv(st)};
}

#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN (-1)
#define RUSAGE_THREAD   1

long sys_getrusage(long who, void *uru) {
    if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN && who != RUSAGE_THREAD) {
        return -EINVAL;
    }
    struct k_rusage ru;
    time_rusage(curproc(), who == RUSAGE_CHILDREN, &ru);
    return copy_to_user(uru, &ru, sizeof(ru));
}
