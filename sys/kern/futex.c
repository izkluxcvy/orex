#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <futex.h>
#include <ktime.h>
#include <proc.h>
#include <sched.h>
#include <signal.h>
#include <spinlock.h>
#include <thread.h>
#include <uaccess.h>
#include <vm.h>
#include <waitq.h>

struct waiter {
    void *vm;
    uintptr_t addr;
    uint32_t bitset;
    int woken;
    struct waitq wq;
    struct waiter *next;
};

static struct waiter *waiters;

static int wake_locked(void *vm, uintptr_t addr, int n, uint32_t bitset) {
    int done = 0;
    for (struct waiter **pp = &waiters; *pp && done < n;) {
        struct waiter *w = *pp;
        if (w->vm == vm && w->addr == addr && (w->bitset & bitset)) {
            *pp = w->next;
            w->woken = 1;
            waitq_wakeup_all(&w->wq);
            done++;
        } else {
            pp = &w->next;
        }
    }
    return done;
}

int futex_wake(struct vmspace *vm, uintptr_t addr, int n) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    int done = wake_locked(vm, addr, n, FUTEX_BITSET_MATCH_ANY);
    spin_unlock_irqrestore(&sched_lock, flags);
    return done;
}

static int load_locked(uint32_t *uaddr, uint32_t *out) {
    return __uaccess_copy(out, uaddr, sizeof(*out)) != 0;
}

static long check(uint32_t *uaddr) {
    if ((uintptr_t)uaddr & 3) {
        return -EINVAL;
    }
    return uaccess_prepare(uaddr, sizeof(*uaddr), 0) ? -EFAULT : 0;
}

struct key {
    void *space;
    uint64_t off;
};

static struct key key_of(uint32_t *uaddr, int private) {
    struct key k;
    vm_futex_key(curproc()->vm, (uintptr_t)uaddr, private, &k.space, &k.off);
    return k;
}

static long wait(uint32_t *uaddr, uint32_t val, uint64_t deadline,
                 uint32_t bitset, int private) {
    struct key k = key_of(uaddr, private);
    struct waiter w = {.vm = k.space, .addr = k.off, .bitset = bitset};
    waitq_init(&w.wq);
    uint64_t flags;
    uint32_t now;
    while (1) {
        long err = check(uaddr);
        if (err) {
            return err;
        }
        flags = spin_lock_irqsave(&sched_lock);
        if (!load_locked(uaddr, &now)) {
            break;
        }
        spin_unlock_irqrestore(&sched_lock, flags);
    }
    long err = 0;
    if (now != val) {
        err = -EAGAIN;
    } else {
        w.next = waiters;
        waiters = &w;
        int r =
            deadline ? sleep_until(&w.wq, deadline) : waitq_sleep_intr(&w.wq);
        if (!w.woken) {
            for (struct waiter **pp = &waiters; *pp; pp = &(*pp)->next) {
                if (*pp == &w) {
                    *pp = w.next;
                    break;
                }
            }
            err = r == -ETIMEDOUT ? -ETIMEDOUT
                  : deadline      ? -EINTR
                                  : -ERESTARTSYS;
        }
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    return err;
}

static long deadline_of(const void *utimeout, int absolute, int realtime,
                        uint64_t *deadline) {
    *deadline = 0;
    if (!utimeout) {
        return 0;
    }
    struct k_timespec ts;
    if (copy_from_user(&ts, utimeout, sizeof(ts)) != 0) {
        return -EFAULT;
    }
    int64_t ns =
        ts.sec > INT64_MAX / NSEC - 1 ? INT64_MAX / 2 : ts.sec * NSEC + ts.nsec;
    int64_t now = realtime ? time_real() : (int64_t)time_mono();
    int64_t rel = absolute ? ns - now : ns;
    if (rel <= 0) {
        *deadline = 1;
        return 0;
    }
    *deadline = time_mono() + (uint64_t)rel;
    return 0;
}

long sys_futex(uint32_t *uaddr, long op, uint32_t val, const void *utimeout,
               uint32_t *uaddr2, uint32_t val3) {
    int cmd = (int)(op & ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME));
    int rt = (op & FUTEX_CLOCK_REALTIME) != 0;
    int priv = (op & FUTEX_PRIVATE_FLAG) != 0;
    uint64_t deadline;
    long err;

    if (rt && cmd != FUTEX_WAIT_BITSET) {
        return -ENOSYS;
    }
    switch (cmd) {
    case FUTEX_WAIT:
        val3 = FUTEX_BITSET_MATCH_ANY;
        [[fallthrough]];
    case FUTEX_WAIT_BITSET:
        if (!val3) {
            return -EINVAL;
        }
        if ((err = deadline_of(utimeout, cmd == FUTEX_WAIT_BITSET, rt,
                               &deadline)) != 0) {
            return err;
        }
        return wait(uaddr, val, deadline, val3, priv);
    case FUTEX_WAKE:
        val3 = FUTEX_BITSET_MATCH_ANY;
        [[fallthrough]];
    case FUTEX_WAKE_BITSET: {
        if (!val3) {
            return -EINVAL;
        }
        struct key k = key_of(uaddr, priv);
        uint64_t flags = spin_lock_irqsave(&sched_lock);
        int n = wake_locked(k.space, k.off,
                            (int)(val > INT32_MAX ? INT32_MAX : val), val3);
        spin_unlock_irqrestore(&sched_lock, flags);
        return n;
    }
    case FUTEX_REQUEUE:
    case FUTEX_CMP_REQUEUE: {
        long move = (long)(uintptr_t)utimeout;
        uint32_t now = val3;
        uint64_t flags;
        if (move < 0 || ((uintptr_t)uaddr2 & 3)) {
            return -EINVAL;
        }
        struct key k1 = key_of(uaddr, priv);
        struct key k2 = key_of(uaddr2, priv);
        while (1) {
            if ((err = check(uaddr)) != 0) {
                return err;
            }
            flags = spin_lock_irqsave(&sched_lock);
            if (cmd == FUTEX_REQUEUE || !load_locked(uaddr, &now)) {
                break;
            }
            spin_unlock_irqrestore(&sched_lock, flags);
        }
        err = 0;
        if (now != val3) {
            err = -EAGAIN;
        }
        if (!err) {
            int n = wake_locked(k1.space, k1.off,
                                (int)(val > INT32_MAX ? INT32_MAX : val),
                                FUTEX_BITSET_MATCH_ANY);
            for (struct waiter *w = waiters; w && move; w = w->next) {
                if (w->vm == k1.space && w->addr == k1.off) {
                    w->vm = k2.space;
                    w->addr = k2.off;
                    move--;
                    n++;
                }
            }
            err = n;
        }
        spin_unlock_irqrestore(&sched_lock, flags);
        return err;
    }
    default:
        return -ENOSYS;
    }
}

/*
 * robust lists
 */

#define FUTEX_WAITERS    0x8000'0000U
#define FUTEX_OWNER_DIED 0x4000'0000U
#define FUTEX_TID_MASK   0x3fff'ffffU
#define ROBUST_LIMIT     2048

struct robust_head {
    uintptr_t list;
    long offset;
    uintptr_t pending;
};

static void robust_one(uintptr_t word, int tid) {
    uint32_t w, nw;
    if ((word & 3) || uaccess_prepare((void *)word, 4, 1) != 0) {
        return;
    }
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    int mine = __uaccess_copy(&w, (void *)word, 4) == 0 &&
               (w & FUTEX_TID_MASK) == (uint32_t)tid;
    if (mine) {
        nw = (w & FUTEX_WAITERS) | FUTEX_OWNER_DIED;
        mine = __uaccess_copy((void *)word, &nw, 4) == 0;
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    if (mine && (w & FUTEX_WAITERS)) {
        struct key k = key_of((uint32_t *)word, 0);
        flags = spin_lock_irqsave(&sched_lock);
        wake_locked(k.space, k.off, 1, FUTEX_BITSET_MATCH_ANY);
        spin_unlock_irqrestore(&sched_lock, flags);
    }
}

void futex_exit_robust(struct thread *t) {
    struct robust_head h;
    uintptr_t head = t->robust_list;
    if (!head || copy_from_user(&h, (void *)head, sizeof(h)) != 0) {
        return;
    }
    uintptr_t e = h.list;
    for (int n = 0; e && e != head && n < ROBUST_LIMIT; n++) {
        uintptr_t next;
        if (copy_from_user(&next, (void *)e, sizeof(next)) != 0) {
            break;
        }
        if (e != h.pending) {
            robust_one(e + (uintptr_t)h.offset, t->utid);
        }
        e = next;
    }
    if (h.pending) {
        robust_one(h.pending + (uintptr_t)h.offset, t->utid);
    }
}
