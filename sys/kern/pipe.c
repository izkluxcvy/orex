#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <kmalloc.h>
#include <memcpy.h>
#include <memset.h>
#include <mutex.h>
#include <sched.h>
#include <spinlock.h>
#include <uaccess.h>
#include <vfs.h>
#include <waitq.h>

#define PIPE_SIZE 65536
#define PIPE_BUF  512

struct pipe {
    char         buf[PIPE_SIZE];
    size_t       head, len;
    struct mutex rlock, wlock;
    int          readers, writers;
    struct waitq can_read, can_write;
};

static int is_reader(struct file *f) {
    return (f->flags & O_ACCMODE) != O_WRONLY;
}
static int is_writer(struct file *f) {
    return (f->flags & O_ACCMODE) != O_RDONLY;
}

static void wake(struct waitq *wq) { waitq_wakeup_all(wq); }

static long wait_data(struct file *f, struct pipe *p, uint64_t *flags) {
    for (int intr = 0; p->len == 0 && p->writers > 0;) {
        if (intr || (f->flags & O_NONBLOCK)) {
            spin_unlock_irqrestore(&sched_lock, *flags);
            return intr ? -EINTR : -EAGAIN;
        }
        intr = waitq_sleep_intr(&p->can_read) != 0;
    }
    return 0;
}

static long pipe_read(struct file *f, void *ubuf, size_t n) {
    struct pipe *p = f->data;
    if (n == 0) {
        return 0;
    }
    uint64_t flags;
    while (1) {
        flags    = spin_lock_irqsave(&sched_lock);
        long err = wait_data(f, p, &flags);
        if (err) {
            return err;
        }
        spin_unlock_irqrestore(&sched_lock, flags);
        mutex_lock(&p->rlock);
        flags = spin_lock_irqsave(&sched_lock);
        if (p->len || !p->writers) {
            break;
        }
        spin_unlock_irqrestore(&sched_lock, flags);
        mutex_unlock(&p->rlock);
    }
    size_t head  = p->head;
    size_t avail = p->len < n ? p->len : n;
    spin_unlock_irqrestore(&sched_lock, flags);

    size_t got = 0;
    long   err = 0;
    while (got < avail) {
        size_t chunk =
            PIPE_SIZE - head < avail - got ? PIPE_SIZE - head : avail - got;
        if (copy_to_user((char *)ubuf + got, p->buf + head, chunk) != 0) {
            err = -EFAULT;
            break;
        }
        head = (head + chunk) % PIPE_SIZE;
        got += chunk;
    }

    flags   = spin_lock_irqsave(&sched_lock);
    p->head = (p->head + got) % PIPE_SIZE;
    p->len -= got;
    if (got) {
        wake(&p->can_write);
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    mutex_unlock(&p->rlock);
    return got ? (long)got : err;
}

static long wait_room(struct file *f, struct pipe *p, size_t want,
                      uint64_t *flags) {
    for (int intr = 0; p->readers > 0 && PIPE_SIZE - p->len < want;) {
        if (intr || (f->flags & O_NONBLOCK)) {
            spin_unlock_irqrestore(&sched_lock, *flags);
            return intr ? -EINTR : -EAGAIN;
        }
        intr = waitq_sleep_intr(&p->can_write) != 0;
    }
    if (p->readers == 0) {
        spin_unlock_irqrestore(&sched_lock, *flags);
        return -EPIPE;
    }
    return 0;
}

static long pipe_write(struct file *f, const void *ubuf, size_t n) {
    struct pipe *p    = f->data;
    size_t       done = 0;
    long         err  = 0;
    while (done < n) {
        size_t   want = n - done <= PIPE_BUF ? n - done : 1;
        uint64_t flags;
        while (1) {
            flags = spin_lock_irqsave(&sched_lock);
            if ((err = wait_room(f, p, want, &flags)) != 0) {
                break;
            }
            spin_unlock_irqrestore(&sched_lock, flags);
            mutex_lock(&p->wlock);
            flags = spin_lock_irqsave(&sched_lock);
            if (!p->readers || PIPE_SIZE - p->len >= want) {
                break;
            }
            spin_unlock_irqrestore(&sched_lock, flags);
            mutex_unlock(&p->wlock);
        }
        if (err) {
            break;
        }
        if (!p->readers) {
            spin_unlock_irqrestore(&sched_lock, flags);
            mutex_unlock(&p->wlock);
            err = -EPIPE;
            break;
        }
        size_t tail = (p->head + p->len) % PIPE_SIZE;
        size_t room = PIPE_SIZE - p->len;
        size_t k    = room < n - done ? room : n - done;
        spin_unlock_irqrestore(&sched_lock, flags);

        size_t put = 0;
        while (put < k) {
            size_t chunk =
                PIPE_SIZE - tail < k - put ? PIPE_SIZE - tail : k - put;
            if (copy_from_user(p->buf + tail, (const char *)ubuf + done + put,
                               chunk) != 0) {
                err = -EFAULT;
                break;
            }
            tail = (tail + chunk) % PIPE_SIZE;
            put += chunk;
        }

        flags = spin_lock_irqsave(&sched_lock);
        p->len += put;
        if (put) {
            wake(&p->can_read);
        }
        spin_unlock_irqrestore(&sched_lock, flags);
        mutex_unlock(&p->wlock);
        done += put;
        if (err) {
            break;
        }
    }
    return done ? (long)done : err;
}

static void pipe_close(struct file *f) {
    struct pipe *p     = f->data;
    uint64_t     flags = spin_lock_irqsave(&sched_lock);

    if (is_reader(f)) {
        p->readers--;
        wake(&p->can_write);
    }
    if (is_writer(f)) {
        p->writers--;
        wake(&p->can_read);
    }
    int last = (p->readers == 0 && p->writers == 0);

    spin_unlock_irqrestore(&sched_lock, flags);
    if (last) {
        kfree(p);
    }
}

static const struct file_ops pipe_ops = {
    .read  = pipe_read,
    .write = pipe_write,
    .close = pipe_close,
};

static struct pipe *pipe_new() {
    struct pipe *p = kmalloc(sizeof(*p));
    if (p) {
        memset(p, 0, sizeof(*p));
        mutex_init(&p->rlock);
        mutex_init(&p->wlock);
        waitq_init(&p->can_read);
        waitq_init(&p->can_write);
    }
    return p;
}

int pipe_create(struct file *out[2]) {
    struct pipe *p = pipe_new();
    if (!p) {
        return -ENOMEM;
    }
    p->readers = p->writers = 1;

    out[0] = file_new(&pipe_ops, O_RDONLY, p);
    out[1] = file_new(&pipe_ops, O_WRONLY, p);
    if (!out[0] || !out[1]) {
        kfree(out[0]);
        kfree(out[1]);
        kfree(p);
        return -ENOMEM;
    }
    return 0;
}
