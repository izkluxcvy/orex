#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <kmalloc.h>
#include <ktime.h>
#include <memset.h>
#include <proc.h>
#include <sched.h>
#include <signal.h>
#include <spinlock.h>
#include <thread.h>
#include <uaccess.h>
#include <waitq.h>

#include "user.h"

#define UNBLOCKABLE (SIGBIT(SIGKILL) | SIGBIT(SIGSTOP))

struct sigq {
    struct siginfo info;
    struct sigq   *next;
};

struct sigset_ref {
    uint64_t       *pending;
    struct siginfo *info;
    struct sigq   **queue;
};

static struct sigset_ref of_proc(struct proc *p) {
    return (struct sigset_ref){&p->sig_pending, p->sig_info, &p->sig_queue};
}

static struct sigset_ref of_thread(struct thread *t) {
    return (struct sigset_ref){&t->sig.pending, t->sig.info, &t->sig.queue};
}

static void drop_all(struct proc *p, struct sigset_ref s, int sig);

static int valid(long sig) { return sig > 0 && sig < NSIG; }

static int ignored_by_default(int sig) {
    switch (sig) {
    case SIGCHLD:
    case SIGCONT:
    case SIGURG:
    case SIGWINCH:
        return 1;
    default:
        return 0;
    }
}

static int is_stop(int sig) {
    return sig == SIGSTOP || sig == SIGTSTP || sig == SIGTTIN || sig == SIGTTOU;
}

static uint64_t pending_for(struct thread *t) {
    return t->sig.pending | t->proc->sig_pending;
}

static void kick(struct thread *t) {
    if (t->state == THREAD_BLOCKED && t->sleep_wq) {
        waitq_remove(t->sleep_wq, t);
        t->sleep_wq = nullptr;
        sched_enqueue(t);
    }
}

void signal_kick_others_locked(struct proc *p) {
    for (struct thread *t = p->threads; t; t = t->proc_next) {
        if (t != curthread) {
            kick(t);
        }
    }
    waitq_wakeup_all(&p->stop_wait);
}

static void tell_parent(struct proc *p, int code, int status) {
    struct proc *up = p->parent;
    if (!up) {
        return;
    }
    waitq_wakeup_all(&up->child_exit);
    const struct k_sigaction *act = &up->sig_actions[SIGCHLD];
    if (!(act->flags & SA_NOCLDSTOP)) {
        struct siginfo info = {.si_signo = SIGCHLD,
                               .si_code  = code,
                               .pid      = p->pid,
                               .status   = status};
        signal_send_info_locked(up, &info);
    }
}

static void continue_locked(struct proc *p) {
    if (p->stopped) {
        p->stopped     = 0;
        p->stop_report = 0;
        p->cont_report = 1;
        waitq_wakeup_all(&p->stop_wait);
        tell_parent(p, CLD_CONTINUED, SIGCONT);
    }
}

int signal_ignored_or_blocked(struct proc *p, int sig) {
    return p->sig_actions[sig].handler == SIG_IGN ||
           (curthread->sig.blocked & SIGBIT(sig));
}

static int discarded(struct proc *p, int sig) {
    uintptr_t h = p->sig_actions[sig].handler;
    if (p->pid == 1 && h == SIG_DFL) {
        return 1;
    }
    if (sig == SIGKILL) {
        return 0;
    }
    return h == SIG_IGN || (h == SIG_DFL && ignored_by_default(sig));
}

static int takes(struct thread *t, uint64_t bit) {
    return (bit & ~t->sig.blocked) || (bit & t->sig.waitset);
}

static void wake_for(struct proc *p, int sig) {
    uint64_t bit = SIGBIT(sig);
    for (struct thread *t = p->threads; t; t = t->proc_next) {
        if (takes(t, bit) && !(t->state == THREAD_BLOCKED && !t->sleep_wq)) {
            kick(t);
            return;
        }
    }
    for (struct thread *t = p->threads; t; t = t->proc_next) {
        if (takes(t, bit)) {
            return;
        }
    }
}

static void stop_or_continue(struct proc *p, int sig) {
    if (sig == SIGCONT) {
        continue_locked(p);
        for (int s = SIGSTOP; s <= SIGTTOU; s++) {
            if (is_stop(s)) {
                drop_all(p, of_proc(p), s);
                for (struct thread *t = p->threads; t; t = t->proc_next) {
                    drop_all(p, of_thread(t), s);
                }
            }
        }
    } else if (is_stop(sig)) {
        drop_all(p, of_proc(p), SIGCONT);
    } else if (sig == SIGKILL && p->stopped) {
        waitq_wakeup_all(&p->stop_wait);
    }
}

static int enqueue(struct proc *p, struct sigset_ref s,
                   const struct siginfo *info) {
    int sig = info->si_signo;
    if (sig < SIGRTMIN) {
        int merged = (*s.pending & SIGBIT(sig)) != 0;
        if (!merged) {
            s.info[sig] = *info;
        }
        *s.pending |= SIGBIT(sig);
        return !merged;
    }
    struct sigq *q =
        p->sig_queued < SIGQUEUE_MAX ? kmalloc(sizeof(*p)) : nullptr;
    if (!q) {
        return -EAGAIN;
    }
    q->info            = *info;
    q->next            = nullptr;
    struct sigq **tail = s.queue;
    while (*tail) {
        tail = &(*tail)->next;
    }
    *tail = q;
    p->sig_queued++;
    *s.pending |= SIGBIT(sig);
    return 1;
}

int signal_send_info_locked(struct proc *p, const struct siginfo *info) {
    int sig = info->si_signo;
    if (p->state != PROC_ALIVE) {
        return 0;
    }
    stop_or_continue(p, sig);
    if (discarded(p, sig)) {
        return 0;
    }
    int r = enqueue(p, of_proc(p), info);
    if (r > 0 || sig < SIGRTMIN) {
        wake_for(p, sig);
    }
    return r;
}

int signal_send_thread_locked(struct thread *t, const struct siginfo *info) {
    struct proc *p   = t->proc;
    int          sig = info->si_signo;
    if (p->state != PROC_ALIVE) {
        return 0;
    }
    stop_or_continue(p, sig);
    if (discarded(p, sig)) {
        return 0;
    }
    int r = enqueue(p, of_thread(t), info);
    if (takes(t, SIGBIT(sig))) {
        kick(t);
    }
    return r;
}

static struct siginfo kernel_info(int sig) {
    return (struct siginfo){.si_signo = sig, .si_code = SI_KERNEL};
}

void signal_send_locked(struct proc *p, int sig) {
    struct siginfo info = kernel_info(sig);
    signal_send_info_locked(p, &info);
}

void signal_send(struct proc *p, int sig) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    signal_send_locked(p, sig);
    spin_unlock_irqrestore(&sched_lock, flags);
}

int signal_group_locked(int pgid, int sig) {
    int found = 0;
    for (struct proc *p = proc_list; p; p = p->all_next) {
        if (p->pgid == pgid) {
            signal_send_locked(p, sig);
            found = 1;
        }
    }
    return found;
}

static int queued(struct sigset_ref s, int sig) {
    for (struct sigq *q = *s.queue; q; q = q->next) {
        if (q->info.si_signo == sig) {
            return 1;
        }
    }
    return 0;
}

static void dequeue(struct proc *p, struct sigset_ref s, int sig,
                    struct siginfo *out) {
    if (sig < SIGRTMIN) {
        *out = s.info[sig];
        *s.pending &= ~SIGBIT(sig);
        if (out->si_code == SI_TIMER) {
            time_timer_taken(p, out);
        }
        return;
    }

    struct sigq **pp = s.queue;
    while (*pp && (*pp)->info.si_signo != sig) {
        pp = &(*pp)->next;
    }
    struct sigq *q = *pp;
    if (q) {
        *out = q->info;
        *pp  = q->next;
        kfree(q);
        p->sig_queued--;
        if (out->si_code == SI_TIMER) {
            time_timer_taken(p, out);
        }
    } else {
        *out = kernel_info(sig);
    }
    if (!queued(s, sig)) {
        *s.pending &= ~SIGBIT(sig);
    }
}

static void take(struct thread *t, int sig, struct siginfo *out) {
    if (t->sig.pending & SIGBIT(sig)) {
        dequeue(t->proc, of_thread(t), sig, out);
    } else {
        dequeue(t->proc, of_proc(t->proc), sig, out);
    }
}

static void drop_all(struct proc *p, struct sigset_ref s, int sig) {
    while (*s.pending & SIGBIT(sig)) {
        struct siginfo unused;
        dequeue(p, s, sig, &unused);
    }
}

void signal_force(int sig, int code, uintptr_t addr) {
    struct thread *t     = curthread;
    struct proc   *p     = t->proc;
    uint64_t       flags = spin_lock_irqsave(&sched_lock);

    if ((t->sig.blocked & SIGBIT(sig)) ||
        p->sig_actions[sig].handler == SIG_IGN) {
        p->sig_actions[sig].handler = SIG_DFL;
        t->sig.blocked &= ~SIGBIT(sig);
    }
    t->sig.info[sig] =
        (struct siginfo){.si_signo = sig, .si_code = code, .addr = addr};
    t->sig.pending |= SIGBIT(sig);

    spin_unlock_irqrestore(&sched_lock, flags);
}

int signal_pending() {
    struct thread *t = curthread;
    struct proc   *p = t->proc;
    return p && ((pending_for(t) & ~t->sig.blocked) || p->stopped);
}

int signal_killed() {
    struct thread *t = curthread;
    return t->proc && (pending_for(t) & SIGBIT(SIGKILL));
}

static void finish_syscall(void *frame, int handler, uint64_t flags) {
    if (!frame_is_syscall(frame) || curthread->syscall_nr < 0) {
        return;
    }
    long r = frame_retval(frame), again;
    if (r == -ERESTARTNOINTR) {
        again = 1;
    } else if (r == -ERESTARTSYS) {
        again = !handler || (flags & SA_RESTART);
    } else if (r == -ERESTARTNOHAND) {
        again = !handler;
    } else {
        return;
    }

    if (again) {
        frame_restart_syscall(frame, curthread->syscall_nr);
    } else {
        frame_set_retval(frame, -EINTR);
    }
    curthread->syscall_nr = -1;
}

void signal_deliver(void *frame) {
    struct thread *t = curthread;
    struct proc   *p = t->proc;
    if (!p) {
        return;
    }

    while (1) {
        uint64_t flags = spin_lock_irqsave(&sched_lock);
        if (p->stopped && !(pending_for(t) & SIGBIT(SIGKILL))) {
            waitq_sleep(&p->stop_wait);
            spin_unlock_irqrestore(&sched_lock, flags);
            continue;
        }
        uint64_t ready = pending_for(t) & ~t->sig.blocked;
        if (!ready) {
            if (t->sig.restore) {
                t->sig.blocked = t->sig.saved;
                t->sig.restore = 0;
            }
            spin_unlock_irqrestore(&sched_lock, flags);
            finish_syscall(frame, 0, 0);
            return;
        }

        int            sig = __builtin_ctzll(ready) + 1;
        struct siginfo info;
        take(t, sig, &info);
        struct k_sigaction act = p->sig_actions[sig];

        if (act.handler == SIG_IGN ||
            (act.handler == SIG_DFL && ignored_by_default(sig))) {
            spin_unlock_irqrestore(&sched_lock, flags);
            continue;
        }
        if (act.handler == SIG_DFL && is_stop(sig)) {
            if (sig == SIGSTOP || !pgrp_orphaned_locked(p->pgid)) {
                p->stopped     = 1;
                p->stop_sig    = sig;
                p->stop_report = 1;
                p->cont_report = 0;
                tell_parent(p, CLD_STOPPED, sig);
                signal_kick_others_locked(p);
            }
            spin_unlock_irqrestore(&sched_lock, flags);
            continue;
        }
        if (act.handler == SIG_DFL) {
            spin_unlock_irqrestore(&sched_lock, flags);
            proc_exit(W_SIGNALED(sig));
        }

        uint64_t old   = t->sig.restore ? t->sig.saved : t->sig.blocked;
        t->sig.restore = 0;
        t->sig.blocked |= act.mask & ~UNBLOCKABLE;
        if (!(act.flags & SA_NODEFER)) {
            t->sig.blocked |= SIGBIT(sig);
        }
        if (act.flags & SA_RESETHAND) {
            p->sig_actions[sig].handler = SIG_DFL;
            p->sig_actions[sig].flags &= ~(uint16_t)SA_SIGINFO;
        }
        spin_unlock_irqrestore(&sched_lock, flags);

        finish_syscall(frame, 1, act.flags);
        if (signal_frame_push(frame, &act, &info, old, &t->sig.altstack) != 0) {
            proc_exit(W_SIGNALED(SIGSEGV));
        }
        return;
    }
}

void signal_thread_init(struct thread *t, struct thread *from) {
    t->sig          = (struct thread_sig){0};
    t->sig.blocked  = from ? from->sig.blocked : 0;
    t->sig.altstack = (struct k_stack){.flags = SS_DISABLE};
    waitq_init(&t->sig_wait);
}

void signal_thread_exit(struct thread *t) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    while (t->sig.queue) {
        struct sigq *q = t->sig.queue;
        t->sig.queue   = q->next;
        kfree(q);
        t->proc->sig_queued--;
    }
    t->sig.pending = 0;
    spin_unlock_irqrestore(&sched_lock, flags);
}

void signal_fork(struct proc *child, struct proc *parent) {
    for (int sig = 1; sig < NSIG; sig++) {
        child->sig_actions[sig] = parent->sig_actions[sig];
    }
    struct thread *t = child->threads;
    signal_thread_init(t, curthread);
    t->sig.altstack    = curthread->sig.altstack;
    child->sig_pending = 0;
}

void signal_exec(struct proc *p) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    for (int sig = 1; sig < NSIG; sig++) {
        if (p->sig_actions[sig].handler != SIG_IGN) {
            p->sig_actions[sig] = (struct k_sigaction){0};
        }
    }
    curthread->sig.altstack = (struct k_stack){.flags = SS_DISABLE};
    spin_unlock_irqrestore(&sched_lock, flags);
}

void signal_exit(struct proc *p) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    while (p->sig_queue) {
        struct sigq *q = p->sig_queue;
        p->sig_queue   = q->next;
        kfree(q);
    }
    p->sig_queued  = 0;
    p->sig_pending = 0;
    spin_unlock_irqrestore(&sched_lock, flags);
}

static struct siginfo user_info(int sig, int code) {
    struct proc *self = curproc();
    return (struct siginfo){.si_signo = sig, .si_code = code, .pid = self->pid};
}

long sys_kill(long pid, long sig) {
    if (sig != 0 && !valid(sig)) {
        return -EINVAL;
    }
    struct proc   *self    = curproc();
    struct siginfo info    = user_info((int)sig, SI_USER);
    uint64_t       flags   = spin_lock_irqsave(&sched_lock);
    int            found   = 0;
    int            allowed = 0;
    long           err     = 0;

    int pgid = pid == 0 ? self->pgid : (int)-pid;
    for (struct proc *p = proc_list; p; p = p->all_next) {
        int match = pid > 0     ? p->pid == pid
                    : pid == -1 ? p->pid != 1 && p != self
                                : p->pgid == pgid;
        if (!match || p->state != PROC_ALIVE) {
            continue;
        }
        found   = 1;
        allowed = 1;
        if (sig) {
            long r = signal_send_info_locked(p, &info);
            err    = r < 0 ? r : err;
        }
    }

    spin_unlock_irqrestore(&sched_lock, flags);
    return !found ? -ESRCH : !allowed ? -EPERM : err;
}

long sys_tgkill(long tgid, long tid, long sig) {
    if ((sig != 0 && !valid(sig)) || tid <= 0 || (tgid != -1 && tgid <= 0)) {
        return -EINVAL;
    }
    struct siginfo info  = user_info((int)sig, SI_TKILL);
    uint64_t       flags = spin_lock_irqsave(&sched_lock);
    struct thread *t     = proc_find_thread_locked((pid_t)tid);
    long           err   = 0;
    if (!t || (tgid != -1 && t->proc->pid != tgid) ||
        t->proc->state != PROC_ALIVE) {
        err = -ESRCH;
    } else if (sig) {
        err = signal_send_thread_locked(t, &info);
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    return err < 0 ? err : 0;
}

long sys_rt_sigqueueinfo(long pid, long sig, const struct siginfo *uinfo) {
    struct siginfo info;
    if (copy_from_user(&info, uinfo, sizeof(info)) != 0) {
        return -EFAULT;
    }
    if ((sig != 0 && !valid(sig)) || pid <= 0) {
        return -EINVAL;
    }
    struct proc *self = curproc();
    if (info.si_code >= 0 && pid != self->pid) {
        return -EPERM;
    }
    info.si_signo    = (int)sig;
    info.pid         = self->pid;
    uint64_t     f   = spin_lock_irqsave(&sched_lock);
    struct proc *p   = proc_find_locked((pid_t)pid);
    long         err = !p || p->state != PROC_ALIVE ? -ESRCH
                       : sig ? signal_send_info_locked(p, &info)
                             : 0;
    spin_unlock_irqrestore(&sched_lock, f);
    return err < 0 ? err : 0;
}

long sys_rt_sigaction(long sig, const struct k_sigaction *uact,
                      struct k_sigaction *uold, long setsize) {
    if (!valid(sig) || setsize != sizeof(uint64_t)) {
        return -EINVAL;
    }
    if (uact && (sig == SIGKILL || sig == SIGSTOP)) {
        return -EINVAL;
    }

    struct k_sigaction act;
    if (uact && copy_from_user(&act, uact, sizeof(act)) != 0) {
        return -EFAULT;
    }

    struct proc       *p     = curproc();
    uint64_t           flags = spin_lock_irqsave(&sched_lock);
    struct k_sigaction old   = p->sig_actions[sig];
    if (uact) {
        act.mask &= ~UNBLOCKABLE;
        p->sig_actions[sig] = act;
        if (discarded(p, (int)sig)) {
            drop_all(p, of_proc(p), (int)sig);
            for (struct thread *t = p->threads; t; t = t->proc_next) {
                drop_all(p, of_thread(t), (int)sig);
            }
        }
    }
    spin_unlock_irqrestore(&sched_lock, flags);

    if (uold && copy_to_user(uold, &old, sizeof(old)) != 0) {
        return -EFAULT;
    }
    return 0;
}

long sys_rt_sigprocmask(long how, const uint64_t *uset, uint64_t *uold,
                        long setsize) {
    if (setsize != sizeof(uint64_t)) {
        return -EINVAL;
    }
    uint64_t set = 0;
    if (uset && copy_from_user(&set, uset, sizeof(set)) != 0) {
        return -EFAULT;
    }

    struct thread *t     = curthread;
    uint64_t       flags = spin_lock_irqsave(&sched_lock);
    uint64_t       old   = t->sig.blocked;
    long           err   = 0;

    if (uset) {
        switch (how) {
        case SIG_BLOCK:
            t->sig.blocked |= set;
            break;
        case SIG_UNBLOCK:
            t->sig.blocked &= ~set;
            break;
        case SIG_SETMASK:
            t->sig.blocked = set;
            break;
        default:
            err = -EINVAL;
        }
        t->sig.blocked &= ~UNBLOCKABLE;
        uint64_t moved = t->proc->sig_pending & t->sig.blocked & ~old;
        for (int sig = 1; moved; sig++, moved >>= 1) {
            if (moved & 1) {
                wake_for(t->proc, sig);
            }
        }
    }
    spin_unlock_irqrestore(&sched_lock, flags);

    if (!err && uold && copy_to_user(uold, &old, sizeof(old)) != 0) {
        return -EFAULT;
    }
    return err;
}

long sys_rt_sigpending(uint64_t *uset, long setsize) {
    if (setsize != sizeof(uint64_t)) {
        return -EINVAL;
    }
    struct thread *t   = curthread;
    uint64_t       set = pending_for(t) & t->sig.blocked;
    return copy_to_user(uset, &set, sizeof(set));
}

long sys_rt_sigsuspend(const uint64_t *umask, long setsize) {
    uint64_t mask;
    if (setsize != sizeof(uint64_t)) {
        return -EINVAL;
    }
    if (copy_from_user(&mask, umask, sizeof(mask)) != 0) {
        return -EFAULT;
    }
    struct thread *t     = curthread;
    uint64_t       flags = spin_lock_irqsave(&sched_lock);
    t->sig.saved         = t->sig.blocked;
    t->sig.restore       = 1;
    t->sig.blocked       = mask & ~UNBLOCKABLE;
    while (!signal_pending()) {
        waitq_sleep_intr(&t->sig_wait);
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    return -ERESTARTNOHAND;
}

void signal_mask_during(uint64_t mask) {
    struct thread *t     = curthread;
    uint64_t       flags = spin_lock_irqsave(&sched_lock);
    t->sig.saved         = t->sig.blocked;
    t->sig.restore       = 1;
    t->sig.blocked       = mask & ~UNBLOCKABLE;
    spin_unlock_irqrestore(&sched_lock, flags);
}

void signal_mask_end(long ret) {
    struct thread *t = curthread;
    if (ret == -ERESTARTNOHAND && signal_pending()) {
        return;
    }
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    t->sig.blocked = t->sig.saved;
    t->sig.restore = 0;
    spin_unlock_irqrestore(&sched_lock, flags);
}

long sys_pause() {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    while (!signal_pending()) {
        waitq_sleep_intr(&curthread->sig_wait);
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    return -ERESTARTNOHAND;
}

long sys_rt_sigtimedwait(const uint64_t *uset, struct siginfo *uinfo,
                         const void *utimeout, long setsize) {
    uint64_t          set;
    struct k_timespec ts = {0};
    if (setsize != sizeof(uint64_t)) {
        return -EINVAL;
    }
    if (copy_from_user(&set, uset, sizeof(set)) != 0 ||
        (utimeout && copy_from_user(&ts, utimeout, sizeof(ts)) != 0)) {
        return -EFAULT;
    }
    if (utimeout && (ts.sec < 0 || ts.nsec < 0 || ts.nsec >= NSEC)) {
        return -EINVAL;
    }
    uint64_t deadline = 0;
    if (utimeout) {
        deadline = time_mono() + (uint64_t)(ts.sec * NSEC + ts.nsec);
    }
    set &= ~UNBLOCKABLE;

    struct thread *t = curthread;
    struct siginfo info;
    uint64_t       flags = spin_lock_irqsave(&sched_lock);
    long           ret;
    while (1) {
        uint64_t hit = pending_for(t) & set;
        if (hit) {
            int sig = __builtin_ctzll(hit) + 1;
            take(t, sig, &info);
            ret = sig;
            break;
        }
        if (signal_pending()) {
            ret = -EINTR;
            break;
        }
        t->sig.waitset = set;
        int r          = utimeout ? sleep_until(&t->sig_wait, deadline)
                                  : waitq_sleep_intr(&t->sig_wait);
        t->sig.waitset = 0;
        if (r == -ETIMEDOUT) {
            ret = -EAGAIN;
            break;
        }
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    if (ret > 0 && uinfo && copy_to_user(uinfo, &info, sizeof(info)) != 0) {
        return -EFAULT;
    }
    return ret;
}

long sys_sigaltstack(const struct k_stack *uss, struct k_stack *uold) {
    struct thread *t   = curthread;
    uintptr_t      sp  = frame_user_sp(t->frame);
    struct k_stack cur = t->sig.altstack;
    int            on =
        !(cur.flags & SS_DISABLE) && sp >= cur.sp && sp - cur.sp < cur.size;
    if (uss) {
        struct k_stack ss;
        if (copy_from_user(&ss, uss, sizeof(ss)) != 0) {
            return -EFAULT;
        }
        if (on) {
            return -EPERM;
        }
        if (ss.flags & ~(SS_DISABLE | SS_ONSTACK)) {
            return -EINVAL;
        }
        if (ss.flags & SS_DISABLE) {
            ss = (struct k_stack){.flags = SS_DISABLE};
        } else if (ss.size < MINSIGSTKSZ) {
            return -ENOMEM;
        } else {
            ss.flags = 0;
        }
        t->sig.altstack = ss;
    }
    if (uold) {
        cur.flags = on ? SS_ONSTACK : cur.flags & SS_DISABLE;
        return copy_to_user(uold, &cur, sizeof(cur));
    }
    return 0;
}

long sys_rt_sigreturn() {
    struct thread *t = curthread;
    uint64_t       blocked;
    struct k_stack alt;
    long           rax;
    t->syscall_nr = -1;
    if (signal_frame_pop(t->frame, &blocked, &alt, &rax) != 0) {
        proc_exit(W_SIGNALED(SIGSEGV));
    }
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    t->sig.blocked = blocked & ~UNBLOCKABLE;
    if (!(alt.flags & ~(SS_DISABLE | SS_ONSTACK)) &&
        ((alt.flags & SS_DISABLE) || alt.size >= MINSIGSTKSZ)) {
        t->sig.altstack =
            alt.flags & SS_DISABLE
                ? (struct k_stack){.flags = SS_DISABLE}
                : (struct k_stack){.sp = alt.sp, .size = alt.size};
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    return rax;
}
