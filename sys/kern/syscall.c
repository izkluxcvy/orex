#include <stddef.h>
#include <stdint.h>

#include <blk.h>
#include <cred.h>
#include <errno.h>
#include <exec.h>
#include <fd.h>
#include <kmalloc.h>
#include <ktime.h>
#include <memcpy.h>
#include <memset.h>
#include <printf.h>
#include <proc.h>
#include <signal.h>
#include <syscall.h>
#include <thread.h>
#include <uaccess.h>
#include <vfs.h>
#include <vm.h>

struct upath {
    char         *path;
    struct vnode *base;
};

static long get_path(long dirfd, const char *upath, struct upath *u) {
    u->base = nullptr;
    if (!(u->path = kmalloc(PATH_MAX))) {
        return -ENOMEM;
    }
    long len = copy_str_from_user(u->path, upath, PATH_MAX);
    if (len < 0) {
        kfree(u->path);
        return len;
    }
    if (u->path[0] != '/' && dirfd != AT_FDCWD) {
        struct file *f   = fd_get(dirfd);
        long         err = !f                                      ? -EBADF
                           : !f->vnode || !S_ISDIR(f->vnode->mode) ? -ENOTDIR
                                                                   : 0;
        if (err) {
            kfree(u->path);
            return err;
        }
        u->base = f->vnode;
    }
    return 0;
}

static void put_path(struct upath *u) { kfree(u->path); }

static int empty_path(const char *upath, long flags) {
    char c;
    return (flags & AT_EMPTY_PATH) && copy_from_user(&c, upath, 1) == 0 &&
           c == '\0';
}

static long at_vnode(long dirfd, const char *upath, long flags,
                     struct vnode **out) {
    if (!upath || empty_path(upath, flags)) {
        struct file *f = fd_get(dirfd);
        if (dirfd == AT_FDCWD) {
            return vfs_lookup(nullptr, ".", 0, out);
        }
        if (!f || !f->vnode) {
            return f ? -EINVAL : -EBADF;
        }
        *out = vnode_ref(f->vnode);
        return 0;
    }

    struct upath u;
    long         err = get_path(dirfd, upath, &u);
    if (err) {
        return err;
    }
    err = vfs_lookup(u.base, u.path,
                     (flags & AT_SYMLINK_NOFOLLOW) ? 0 : VFS_FOLLOW, out);
    put_path(&u);
    return err;
}

static long sys_openat(long dirfd, const char *upath, long flags, long mode) {
    struct upath u;
    long         err = get_path(dirfd, upath, &u);
    if (err) {
        return err;
    }
    struct file *f;
    err = vfs_open(u.base, u.path, (int)flags, (uint32_t)mode, &f);
    put_path(&u);
    if (err) {
        return err;
    }
    f->flags &= O_STATUS;
    int fd = fd_install(f, 0, fd_flags_of(flags));
    if (fd < 0) {
        file_unref(f);
    }
    return fd;
}

static void stat_file(struct file *f, struct stat *st) {
    if (f->vnode) {
        vfs_stat(f->vnode, st);
    } else {
        memset(st, 0, sizeof(*st));
        st->st_mode = f->ops->ioctl ? S_IFCHR | 0620 : S_IFIFO | 0600;
    }
}

static long sys_fstat(long fd, struct stat *ust) {
    struct file *f = fd_get(fd);
    if (!f) {
        return -EBADF;
    }
    struct stat st;
    stat_file(f, &st);
    return copy_to_user(ust, &st, sizeof(st));
}

static long sys_fstatat(long dirfd, const char *upath, struct stat *ust,
                        long flags) {
    if (flags & ~(long)(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
        return -EINVAL;
    }
    if (empty_path(upath, flags) && dirfd != AT_FDCWD) {
        return sys_fstat(dirfd, ust);
    }
    struct vnode *vn;
    long          err = at_vnode(dirfd, upath, flags, &vn);
    if (err) {
        return err;
    }
    struct stat st;
    vfs_stat(vn, &st);
    vnode_put(vn);
    return copy_to_user(ust, &st, sizeof(st));
}

struct dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[];
} __attribute__((packed));

#define DENTS_MAX 65536

static long sys_getdents64(long fd, void *ubuf, long count) {
    struct file *f = fd_get_io(fd);
    if (!f) {
        return -EBADF;
    }
    if (!f->vnode || !S_ISDIR(f->vnode->mode)) {
        return -ENOTDIR;
    }
    if (count < 0 || uaccess_prepare(ubuf, (size_t)count, 1) != 0) {
        return -EFAULT;
    }
    if (count > DENTS_MAX) {
        count = DENTS_MAX;
    }

    struct vfs_dirent *de  = kmalloc(sizeof(*de));
    uint8_t           *out = kmalloc(count ? (size_t)count : 1);
    if (!de || !out) {
        kfree(de);
        kfree(out);
        return -ENOMEM;
    }
    long used = 0;

    while (1) {
        uint64_t pos = (uint64_t)f->offset;
        int      got = f->vnode->fs->vops->readdir(f->vnode, &pos, de);
        if (got <= 0) {
            if (got < 0 && used == 0) {
                used = got;
            }
            break;
        }

        size_t namelen = 0;
        while (de->name[namelen]) {
            namelen++;
        }
        size_t reclen =
            (sizeof(struct dirent64) + namelen + 1 + 7) & ~(size_t)7;
        if ((size_t)(count - used) < reclen) {
            if (used == 0) {
                used = -EINVAL;
            }
            break;
        }

        struct dirent64 *d = (struct dirent64 *)(out + used);
        d->d_ino           = de->ino;
        d->d_off           = (int64_t)pos;
        d->d_reclen        = (uint16_t)reclen;
        d->d_type          = de->type;
        memcpy(d->d_name, de->name, namelen + 1);
        used += (long)reclen;
        f->offset = (long)pos;
    }

    kfree(de);
    if (used > 0 && copy_to_user(ubuf, out, (size_t)used) != 0) {
        used = -EFAULT;
    }
    kfree(out);
    if (used > 0) {
        vfs_accessed(f->vnode);
    }
    return used;
}

static long sys_truncate(const char *upath, long size) {
    struct vnode *vn;
    long          err = at_vnode(AT_FDCWD, upath, 0, &vn);
    if (err) {
        return err;
    }
    if (S_ISDIR(vn->mode)) {
        err = -EISDIR;
    } else if ((err = vfs_permission(vn, W_OK, 0)) == 0) {
        err = vfs_truncate(vn, size);
    }
    vnode_put(vn);
    return err;
}

static long sys_ftruncate(long fd, long size) {
    struct file *f = fd_get_io(fd);
    if (!f) {
        return -EBADF;
    }
    if (!f->vnode || !S_ISREG(f->vnode->mode) ||
        (f->flags & O_ACCMODE) == O_RDONLY) {
        return (f->flags & O_ACCMODE) == O_RDONLY ? -EBADF : -EINVAL;
    }
    return vfs_truncate(f->vnode, size);
}

static long sys_faccessat(long dirfd, const char *upath, long mode,
                          long flags) {
    if ((mode & ~(long)(R_OK | W_OK | X_OK)) ||
        (flags & ~(long)(AT_EACCESS | AT_SYMLINK_NOFOLLOW))) {
        return -EINVAL;
    }
    struct upath u;
    long         err = get_path(dirfd, upath, &u);
    if (err) {
        return err;
    }
    int vflags = ((flags & AT_SYMLINK_NOFOLLOW) ? 0 : VFS_FOLLOW) |
                 ((flags & AT_EACCESS) ? 0 : VFS_REAL);
    err        = vfs_access(u.base, u.path, (int)mode, vflags);
    put_path(&u);
    return err;
}

static long attr_at(long dirfd, const char *upath, long flags, int chmod,
                    long a, long b) {
    if (flags & ~(long)(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
        return -EINVAL;
    }
    struct vnode *vn;
    long          err;
    if (!upath) {
        struct file *f = fd_get_io(dirfd);
        if (!f) {
            return -EBADF;
        }
        if (!f->vnode) {
            return -EINVAL;
        }
        vn = vnode_ref(f->vnode);
    } else if ((err = at_vnode(dirfd, upath, flags, &vn)) != 0) {
        return err;
    }
    if (chmod && S_ISLNK(vn->mode)) {
        err = -EOPNOTSUPP;
    } else {
        err = chmod ? vfs_chmod(vn, (uint32_t)a)
                    : vfs_chown(vn, (uint32_t)a, (uint32_t)b);
    }
    vnode_put(vn);
    return err;
}

static long sys_utimensat(long dirfd, const char *upath,
                          const struct k_timespec *utimes, long flags) {
    struct k_timespec ts[2];
    if (flags & ~(long)AT_SYMLINK_NOFOLLOW) {
        return -EINVAL;
    }
    if (utimes && copy_from_user(ts, utimes, sizeof(ts)) != 0) {
        return -EFAULT;
    }
    struct vnode *vn;
    long          err;
    if (!upath) {
        struct file *f = fd_get_io(dirfd);
        if (!f || !f->vnode) {
            return -EBADF;
        }
        vn = vnode_ref(f->vnode);
    } else if ((err = at_vnode(dirfd, upath, flags, &vn)) != 0) {
        return err;
    }
    err = vfs_utimes(vn, utimes ? ts : nullptr);
    vnode_put(vn);
    return err;
}

static long sys_umask(long mask) {
    struct proc *p   = curproc();
    long         old = p->umask;
    p->umask         = (uint32_t)mask & 0777;
    return old;
}

static long sys_chdir(const char *upath) {
    struct vnode *vn;
    long          err = at_vnode(AT_FDCWD, upath, 0, &vn);
    if (err) {
        return err;
    }
    err = vfs_chdir(vn);
    vnode_put(vn);
    return err;
}

static long sys_fchdir(long fd) {
    struct file *f = fd_get(fd);
    if (!f) {
        return -EBADF;
    }
    return f->vnode ? vfs_chdir(f->vnode) : -ENOTDIR;
}

static long sys_getcwd(char *ubuf, long size) {
    if (size <= 0) {
        return size ? -EINVAL : -ERANGE;
    }
    char *buf = kmalloc(PATH_MAX);
    if (!buf) {
        return -ENOMEM;
    }
    long len = vfs_getcwd(buf, (size_t)size);
    if (len > 0 && copy_to_user(ubuf, buf, (size_t)len) != 0) {
        len = -EFAULT;
    }
    kfree(buf);
    return len;
}

static long do_execve(long dirfd, const char *upath, const char *const *uargv,
                      const char *const *uenvp, long flags) {
    if (flags & ~(long)(AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW)) {
        return -EINVAL;
    }
    char *path = kmalloc(PATH_MAX);
    if (!path) {
        return -ENOMEM;
    }
    long          err   = copy_str_from_user(path, upath, PATH_MAX);
    struct vnode *given = nullptr;
    if (err >= 0 && (dirfd != AT_FDCWD || flags)) {
        err = at_vnode(dirfd, upath, flags, &given);
    }
    struct exec_args a;
    if (err >= 0) {
        err = exec_args_from_user(&a, uargv, uenvp);
    }
    if (err >= 0) {
        err = proc_exec(path[0] ? path : "fexecve", given, &a);
        exec_args_free(&a);
    }
    if (given) {
        vnode_put(given);
    }
    kfree(path);
    return err < 0 ? err : 0;
}

static long sys_execve(const char *upath, const char *const *uargv,
                       const char *const *uenvp) {
    return do_execve(AT_FDCWD, upath, uargv, uenvp, 0);
}

#define RUSAGE_SIZE 144

static long sys_wait4(long pid, int *ustatus, long options, void *urusage) {
    if (options & ~(long)(WNOHANG | WUNTRACED | WCONTINUED) ||
        pid == INT32_MIN) {
        return -EINVAL;
    }
    if (ustatus && uaccess_prepare(ustatus, sizeof(int), 1) != 0) {
        return -EFAULT;
    }
    int   status          = 0;
    char  ru[RUSAGE_SIZE] = {0};
    pid_t ret =
        proc_wait((pid_t)pid, &status, (int)options | WEXITED, nullptr, ru);
    if (ret > 0 && ustatus) {
        copy_to_user(ustatus, &status, sizeof(status));
    }
    if (ret >= 0 && urusage && copy_to_user(urusage, ru, sizeof(ru)) != 0) {
        return -EFAULT;
    }
    return ret;
}

#define P_ALL  0
#define P_PID  1
#define P_PGID 2

static long sys_waitid(long idtype, long id, struct siginfo *uinfo,
                       long options, void *urusage) {
    long allowed = WNOHANG | WUNTRACED | WEXITED | WCONTINUED | WNOWAIT;
    if ((options & ~allowed) ||
        ~(options & (WEXITED | WUNTRACED | WCONTINUED))) {
        return -EINVAL;
    }
    pid_t pid;
    switch (idtype) {
    case P_ALL:
        pid = -1;
        break;
    case P_PID:
        if (id <= 0) {
            return -EINVAL;
        }
        pid = (pid_t)id;
        break;
    case P_PGID:
        if (id < 0) {
            return -EINVAL;
        }
        pid = id ? (pid_t)-id : 0;
        break;
    default:
        return -EINVAL;
    }
    struct siginfo info            = {0};
    char           ru[RUSAGE_SIZE] = {0};
    pid_t          ret = proc_wait(pid, nullptr, (int)options, &info, ru);
    if (ret < 0) {
        return ret;
    }
    if (uinfo && copy_to_user(uinfo, &info, sizeof(info)) != 0) {
        return -EFAULT;
    }
    if (urusage && copy_to_user(urusage, ru, sizeof(ru)) != 0) {
        return -EFAULT;
    }
    return 0;
}

long syscall_dispatch(long nr, long a0, long a1, long a2, long a3, long a4,
                      long a5) {
    switch (nr) {
    case SYS_mmap:
        return sys_mmap((uintptr_t)a0, (size_t)a1, a2, a3, a4, a5);
    case SYS_mprotect:
        return sys_mprotect((uintptr_t)a0, (size_t)a1, a2);
    case SYS_munmap:
        return sys_munmap((uintptr_t)a0, (size_t)a1);
    case SYS_brk:
        return sys_brk((uintptr_t)a0);
    case SYS_read:
        return sys_read(a0, (void *)a1, a2);
    case SYS_write:
        return sys_write(a0, (const void *)a1, a2);
    case SYS_readv:
        return sys_readv(a0, (const void *)a1, a2);
    case SYS_writev:
        return sys_writev(a0, (const void *)a1, a2);
    case SYS_pread64:
        return sys_pread64(a0, (void *)a1, a2, a3);
    case SYS_pwrite64:
        return sys_pwrite64(a0, (const void *)a1, a2, a3);
    case SYS_open:
        return sys_openat(AT_FDCWD, (const char *)a0, a1, a2);
    case SYS_openat:
        return sys_openat(a0, (const char *)a1, a2, a3);
    case SYS_close:
        return sys_close(a0);
    case SYS_stat:
        return sys_fstatat(AT_FDCWD, (const char *)a0, (struct stat *)a1, 0);
    case SYS_lstat:
        return sys_fstatat(AT_FDCWD, (const char *)a0, (struct stat *)a1,
                           AT_SYMLINK_NOFOLLOW);
    case SYS_fstat:
        return sys_fstat(a0, (struct stat *)a1);
    case SYS_newfstatat:
        return sys_fstatat(a0, (const char *)a1, (struct stat *)a2, a3);
    case SYS_getdents64:
        return sys_getdents64(a0, (void *)a1, a2);
    case SYS_access:
        return sys_faccessat(AT_FDCWD, (const char *)a0, a1, 0);
    case SYS_faccessat:
        return sys_faccessat(a0, (const char *)a1, a2, 0);
    case SYS_faccessat2:
        return sys_faccessat(a0, (const char *)a1, a2, a3);
    case SYS_chmod:
        return attr_at(AT_FDCWD, (const char *)a0, 0, 1, a1, 0);
    case SYS_fchmod:
        return attr_at(a0, nullptr, 0, 1, a1, 0);
    case SYS_fchmodat:
        return attr_at(a0, (const char *)a1, 0, 1, a2, 0);
    case SYS_truncate:
        return sys_truncate((const char *)a0, a1);
    case SYS_ftruncate:
        return sys_ftruncate(a0, a1);
    case SYS_fchmodat2:
        return attr_at(a0, (const char *)a1, a3, 1, a2, 0);
    case SYS_chown:
        return attr_at(AT_FDCWD, (const char *)a0, 0, 0, a1, a2);
    case SYS_lchown:
        return attr_at(AT_FDCWD, (const char *)a0, AT_SYMLINK_NOFOLLOW, 0, a1,
                       a2);
    case SYS_fchown:
        return attr_at(a0, nullptr, 0, 0, a1, a2);
    case SYS_fchownat:
        return attr_at(a0, (const char *)a1, a4, 0, a2, a3);
    case SYS_utimensat:
        return sys_utimensat(a0, (const char *)a1,
                             (const struct k_timespec *)a2, a3);
    case SYS_fcntl:
        return sys_fcntl(a0, a1, a2);
    case SYS_fsync:
    case SYS_fdatasync:
        return sys_fsync(a0);
    case SYS_sync:
        return blk_sync(), 0;
    case SYS_umask:
        return sys_umask(a0);
    case SYS_getuid:
        return sys_getuid();
    case SYS_geteuid:
        return sys_geteuid();
    case SYS_getgid:
        return sys_getgid();
    case SYS_getegid:
        return sys_getegid();
    case SYS_setuid:
        return sys_setuid(a0);
    case SYS_setgid:
        return sys_setgid(a0);
    case SYS_setreuid:
        return sys_setreuid(a0, a1);
    case SYS_setregid:
        return sys_setregid(a0, a1);
    case SYS_setresuid:
        return sys_setresuid(a0, a1, a2);
    case SYS_setresgid:
        return sys_setresgid(a0, a1, a2);
    case SYS_getresuid:
        return sys_getresuid((uid_t *)a0, (uid_t *)a1, (uid_t *)a2);
    case SYS_getresgid:
        return sys_getresgid((gid_t *)a0, (gid_t *)a1, (gid_t *)a2);
    case SYS_getgroups:
        return sys_getgroups(a0, (gid_t *)a1);
    case SYS_setgroups:
        return sys_setgroups(a0, (const gid_t *)a1);
    case SYS_chdir:
        return sys_chdir((const char *)a0);
    case SYS_fchdir:
        return sys_fchdir(a0);
    case SYS_getcwd:
        return sys_getcwd((char *)a0, a1);
    case SYS_lseek:
        return sys_lseek(a0, a1, a2);
    case SYS_ioctl:
        return sys_ioctl(a0, a1, (void *)a2);
    case SYS_rt_sigaction:
        return sys_rt_sigaction(a0, (const struct k_sigaction *)a1,
                                (struct k_sigaction *)a2, a3);
    case SYS_rt_sigprocmask:
        return sys_rt_sigprocmask(a0, (const uint64_t *)a1, (uint64_t *)a2, a3);
    case SYS_rt_sigreturn:
        return sys_rt_sigreturn();
    case SYS_rt_sigpending:
        return sys_rt_sigpending((uint64_t *)a0, a1);
    case SYS_rt_sigsuspend:
        return sys_rt_sigsuspend((const uint64_t *)a0, a1);
    case SYS_rt_sigtimedwait:
        return sys_rt_sigtimedwait((const uint64_t *)a0, (struct siginfo *)a1,
                                   (const void *)a2, a3);
    case SYS_rt_sigqueueinfo:
        return sys_rt_sigqueueinfo(a0, a1, (const struct siginfo *)a2);
    case SYS_sigaltstack:
        return sys_sigaltstack((const struct k_stack *)a0,
                               (struct k_stack *)a1);
    case SYS_pause:
        return sys_pause();
    case SYS_tkill:
        return sys_tgkill(-1, a1, a2);
    case SYS_tgkill:
        return sys_tgkill(a0, a1, a2);
    case SYS_clock_gettime:
        return sys_clock_gettime(a0, (struct k_timespec *)a1);
    case SYS_clock_settime:
        return sys_clock_settime(a0, (const struct k_timespec *)a1);
    case SYS_clock_getres:
        return sys_clock_getres(a0, (struct k_timespec *)a1);
    case SYS_nanosleep:
        return sys_nanosleep((const struct k_timespec *)a0,
                             (struct k_timespec *)a1);
    case SYS_clock_nanosleep:
        return sys_clock_nanosleep(a0, a1, (const struct k_timespec *)a2,
                                   (struct k_timespec *)a3);
    case SYS_getitimer:
        return sys_getitimer(a0, (void *)a1);
    case SYS_setitimer:
        return sys_setitimer(a0, (const void *)a1, (void *)a2);
    case SYS_timer_create:
        return sys_timer_create(a0, (const void *)a1, (int *)a2);
    case SYS_timer_settime:
        return sys_timer_settime(a0, a1, (const void *)a2, (void *)a3);
    case SYS_timer_gettime:
        return sys_timer_gettime(a0, (void *)a1);
    case SYS_timer_getoverrun:
        return sys_timer_getoverrun(a0);
    case SYS_timer_delete:
        return sys_timer_delete(a0);
    case SYS_times:
        return sys_times((void *)a0);
    case SYS_getrusage:
        return sys_getrusage(a0, (void *)a1);
    case SYS_kill:
        return sys_kill(a0, a1);
    case SYS_setpgid:
        return proc_setpgid((pid_t)a0, (pid_t)a1);
    case SYS_setsid:
        return proc_setsid();
    case SYS_getsid:
        return proc_getsid((pid_t)a0);
    case SYS_getpgid:
        return proc_getpgid((pid_t)a0);
    case SYS_gettid:
        return curthread->utid;
    case SYS_pipe:
        return sys_pipe2((int *)a0, 0);
    case SYS_pipe2:
        return sys_pipe2((int *)a0, a1);
    case SYS_dup:
        return sys_dup(a0);
    case SYS_dup2:
        return sys_dup3(a0, a1, 0, 1);
    case SYS_dup3:
        return sys_dup3(a0, a1, a2, 0);
    case SYS_getpid:
        return curproc()->pid;
    case SYS_getppid:
        return curproc()->parent ? curproc()->parent->pid : 0;
    case SYS_sched_yield:
        thread_yield();
        return 0;
    case SYS_fork:
        return proc_fork();
    case SYS_execve:
        return sys_execve((const char *)a0, (const char *const *)a1,
                          (const char *const *)a2);
    case SYS_execveat:
        return do_execve(a0, (const char *)a1, (const char *const *)a2,
                         (const char *const *)a3, a4);
    case SYS_wait4:
        return sys_wait4(a0, (int *)a1, a2, (void *)a3);
    case SYS_waitid:
        return sys_waitid(a0, a1, (struct siginfo *)a2, a3, (void *)a4);
    case SYS_exit:
        proc_thread_exit(W_EXITCODE((int)a0));
    case SYS_exit_group:
        proc_exit(W_EXITCODE((int)a0));
    default:
        return -ENOSYS;
    }
}
