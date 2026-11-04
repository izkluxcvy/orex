#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <syscall.h>

int linkat(int olddirfd, const char *old, int newdirfd, const char *new,
           int flags) {
    return (int)__syscall_ret(__syscall5(SYS_linkat, olddirfd, (long)old,
                                         newdirfd, (long)new, flags));
}

int link(const char *old, const char *new) {
    return linkat(AT_FDCWD, old, AT_FDCWD, new, 0);
}

int symlinkat(const char *target, int dirfd, const char *path) {
    return (int)__syscall_ret(
        __syscall3(SYS_symlinkat, (long)target, dirfd, (long)path));
}

int symlink(const char *target, const char *path) {
    return symlinkat(target, AT_FDCWD, path);
}

ssize_t readlinkat(int dirfd, const char *__restrict path, char *__restrict buf,
                   size_t size) {
    return __syscall_ret(
        __syscall4(SYS_readlinkat, dirfd, (long)path, (long)buf, size));
}

ssize_t readlink(const char *__restrict path, char *__restrict buf,
                 size_t size) {
    return readlinkat(AT_FDCWD, path, buf, size);
}

int truncate(const char *path, off_t len) {
    return (int)__syscall_ret(__syscall2(SYS_truncate, (long)path, len));
}

int ftruncate(int fd, off_t len) {
    return (int)__syscall_ret(__syscall2(SYS_ftruncate, fd, len));
}

int lockf(int fd, int cmd, off_t len) {
    struct flock fl = {.l_type = F_WRLCK, .l_whence = SEEK_CUR, .l_len = len};
    switch (cmd) {
    case F_ULOCK:
        fl.l_type = F_UNLCK;
        return fcntl(fd, F_SETLK, &fl);
    case F_LOCK:
        return fcntl(fd, F_SETLKW, &fl);
    case F_TLOCK:
        return fcntl(fd, F_SETLK, &fl);
    case F_TEST:
        fl.l_type = F_WRLCK;
        if (fcntl(fd, F_GETLK, &fl) != 0) {
            return -1;
        }
        if (fl.l_type == F_UNLCK || fl.l_pid == getpid()) {
            return 0;
        }
        errno = EACCES;
        return -1;
    default:
        errno = EINVAL;
        return -1;
    }
}
