#include <fcntl.h>
#include <sys/stat.h>

#include "syscall.h"

int mkdirat(int dirfd, const char *path, mode_t mode) {
    return (int)__syscall_ret(__syscall3(SYS_mkdirat, dirfd, (long)path, mode));
}

int mkdir(const char *path, mode_t mode) {
    return mkdirat(AT_FDCWD, path, mode);
}

int mknodat(int dirfd, const char *path, mode_t mode, dev_t dev) {
    return (int)__syscall_ret(
        __syscall4(SYS_mknodat, dirfd, (long)path, mode, (long)dev));
}

int mknod(const char *path, mode_t mode, dev_t dev) {
    return mknodat(AT_FDCWD, path, mode, dev);
}

int mkfifoat(int dirfd, const char *path, mode_t mode) {
    return mknodat(dirfd, path, (mode & 07777) | S_IFIFO, 0);
}

int mkfifo(const char *path, mode_t mode) {
    return mkfifoat(AT_FDCWD, path, mode);
}
