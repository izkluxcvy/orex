#include <fcntl.h>
#include <sys/stat.h>

#include <syscall.h>

int fstatat(int dirfd, const char *__restrict path, struct stat *__restrict st,
            int flags) {
    return (int)__syscall_ret(
        __syscall4(SYS_newfstatat, dirfd, (long)path, (long)st, flags));
}

int stat(const char *__restrict path, struct stat *__restrict st) {
    return fstatat(AT_FDCWD, path, st, 0);
}

int lstat(const char *__restrict path, struct stat *__restrict st) {
    return fstatat(AT_FDCWD, path, st, AT_SYMLINK_NOFOLLOW);
}

int fstat(int fd, struct stat *st) {
    return (int)__syscall_ret(__syscall2(SYS_fstat, fd, (long)st));
}
