#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <fd.h>
#include <proc.h>
#include <thread.h>
#include <uaccess.h>

static_assert(O_CLOEXEC == 02000000 && O_CLOFORK == 040000000,
              "fd_flags_of has these bits");
#include <vfs.h>

struct file *fd_get(long fd) {
    struct thread *t = curthread;
    struct proc   *p = curproc();
    if (fd < 0 || fd >= PROC_FD_MAX || t->nborrowed == THREAD_BORROW_MAX) {
        return nullptr;
    }
    uint64_t     flags = spin_lock_irqsave(&p->files_lock);
    struct file *f     = p->fds[fd];
    if (f) {
        file_ref(f);
        t->borrowed[t->nborrowed++] = f;
    }
    spin_unlock_irqrestore(&p->files_lock, flags);
    return f;
}

void fd_return_borrowed() {
    struct thread *t = curthread;
    while (t->nborrowed) {
        file_unref(t->borrowed[--t->nborrowed]);
    }
}

struct file *fd_get_io(long fd) {
    struct file *f = fd_get(fd);
    return f && !(f->flags & O_PATH) ? f : nullptr;
}

int fd_install(struct file *f, long min, int fdflags) {
    struct proc *p     = curproc();
    uint64_t     flags = spin_lock_irqsave(&p->files_lock);
    for (long fd = min; fd < PROC_FD_MAX; fd++) {
        if (!p->fds[fd]) {
            p->fds[fd]      = f;
            p->fd_flags[fd] = (uint8_t)fdflags;
            spin_unlock_irqrestore(&p->files_lock, flags);
            return (int)fd;
        }
    }
    spin_unlock_irqrestore(&p->files_lock, flags);
    return -EMFILE;
}

static struct file *fd_take(struct proc *p, long fd) {
    uint64_t     flags = spin_lock_irqsave(&p->files_lock);
    struct file *f     = p->fds[fd];
    p->fds[fd]         = nullptr;
    p->fd_flags[fd]    = 0;
    spin_unlock_irqrestore(&p->files_lock, flags);
    return f;
}

static void drop(struct proc *p, struct file *f) {
    (void)p;
    file_unref(f);
}

static int fd_release(struct proc *p, int fd) {
    struct file *f = fd_take(p, fd);
    if (f) {
        drop(p, f);
    }
    return f ? 0 : -EBADF;
}

void fd_close_all(struct proc *p) {
    for (int fd = 0; fd < PROC_FD_MAX; fd++) {
        if (p->fds[fd]) {
            fd_release(p, fd);
        }
    }
}

void fd_close_on_exec(struct proc *p) {
    for (int fd = 0; fd < PROC_FD_MAX; fd++) {
        if (p->fds[fd] && (p->fd_flags[fd] & FD_CLOEXEC)) {
            fd_release(p, fd);
        }
    }
}

void fd_fork(struct proc *child, struct proc *parent) {
    uint64_t flags = spin_lock_irqsave(&parent->files_lock);
    for (int fd = 0; fd < PROC_FD_MAX; fd++) {
        if (parent->fds[fd] && !(parent->fd_flags[fd] & FD_CLOFORK)) {
            child->fds[fd]      = file_ref(parent->fds[fd]);
            child->fd_flags[fd] = parent->fd_flags[fd];
        }
    }
    spin_unlock_irqrestore(&parent->files_lock, flags);
}

long sys_read(long fd, void *buf, long n) {
    struct file *f = fd_get_io(fd);
    if (!f || (f->flags & O_ACCMODE) == O_WRONLY) {
        return -EBADF;
    }
    if (n < 0 || uaccess_prepare(buf, (size_t)n, 1) != 0) {
        return -EFAULT;
    }
    return f->ops->read(f, buf, (size_t)n);
}

long sys_write(long fd, const void *buf, long n) {
    struct file *f = fd_get_io(fd);
    if (!f || (f->flags & O_ACCMODE) == O_RDONLY || !f->ops->write) {
        return -EBADF;
    }
    if (n < 0 || uaccess_prepare(buf, (size_t)n, 0) != 0) {
        return -EFAULT;
    }
    long ret = f->ops->write(f, buf, (size_t)n);
    return ret;
}

long sys_lseek(long fd, long offset, long whence) {
    struct file *f = fd_get_io(fd);
    return f ? vfs_lseek(f, offset, (int)whence) : -EBADF;
}

long sys_close(long fd) {
    if (fd < 0 || fd >= PROC_FD_MAX) {
        return -EBADF;
    }
    return fd_release(curproc(), (int)fd);
}

long sys_dup(long fd) {
    struct file *f = fd_get(fd);
    if (!f) {
        return -EBADF;
    }
    int nfd = fd_install(file_ref(f), 0, 0);
    if (nfd < 0) {
        file_unref(f);
    }
    return nfd;
}

long sys_dup3(long fd, long nfd, long flags, int allow_same) {
    struct file *f = fd_get(fd);
    if (!f || nfd < 0 || nfd >= PROC_FD_MAX) {
        return -EBADF;
    }
    if (fd == nfd) {
        return allow_same ? nfd : -EINVAL;
    }
    if (flags & ~(long)(O_CLOEXEC | O_CLOFORK)) {
        return -EINVAL;
    }
    struct proc *p = curproc();
    file_ref(f);
    uint64_t     lf  = spin_lock_irqsave(&p->files_lock);
    struct file *old = p->fds[nfd];
    p->fds[nfd]      = f;
    p->fd_flags[nfd] = (uint8_t)fd_flags_of(flags);
    spin_unlock_irqrestore(&p->files_lock, lf);
    if (old) {
        drop(p, old);
    }
    return nfd;
}

long sys_fcntl(long fd, long cmd, long arg) {
    struct proc *p = curproc();
    struct file *f = fd_get(fd);
    if (!f) {
        return -EBADF;
    }
    switch (cmd) {
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
    case F_DUPFD_CLOFORK: {
        if (arg < 0 || arg >= PROC_FD_MAX) {
            return -EINVAL;
        }
        int nfd = fd_install(file_ref(f), arg,
                             cmd == F_DUPFD_CLOEXEC   ? FD_CLOEXEC
                             : cmd == F_DUPFD_CLOFORK ? FD_CLOFORK
                                                      : 0);
        if (nfd < 0) {
            file_unref(f);
        }
        return nfd;
    }
    case F_GETFD:
        return p->fd_flags[fd];
    case F_SETFD:
        p->fd_flags[fd] = (uint8_t)(arg & (FD_CLOEXEC | FD_CLOFORK));
        return 0;
    case F_GETFL:
        return f->flags;
    case F_SETFL:
        if (f->flags & O_PATH) {
            return -EBADF;
        }
        f->flags = (f->flags & ~O_SETTABLE) | ((int)arg & O_SETTABLE);
        return 0;
    default:
        return -EINVAL;
    }
}
