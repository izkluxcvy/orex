#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <exec.h>
#include <fd.h>
#include <kmalloc.h>
#include <memcpy.h>
#include <memset.h>
#include <printf.h>
#include <proc.h>
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
    return used;
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
    case SYS_fcntl:
        return sys_fcntl(a0, a1, a2);
    case SYS_chdir:
        return sys_chdir((const char *)a0);
    case SYS_fchdir:
        return sys_fchdir(a0);
    case SYS_getcwd:
        return sys_getcwd((char *)a0, a1);
    case SYS_lseek:
        return sys_lseek(a0, a1, a2);
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
    case SYS_exit:
        proc_thread_exit(W_EXITCODE((int)a0));
    case SYS_exit_group:
        proc_exit(W_EXITCODE((int)a0));
    default:
        return -ENOSYS;
    }
}
