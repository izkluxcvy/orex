#include <fcntl.h>
#include <sys/stat.h>

#include "syscall.h"

int fchmodat(int dirfd, const char *path, mode_t mode, int flags) {
    if (flags) {
        return (int)__syscall_ret(
            __syscall4(SYS_fchmodat2, dirfd, (long)path, mode, flags));
    }
    return (int)__syscall_ret(
        __syscall3(SYS_fchmodat, dirfd, (long)path, mode));
}

int chmod(const char *path, mode_t mode) {
    return fchmodat(AT_FDCWD, path, mode, 0);
}

int fchmod(int fd, mode_t mode) {
    return (int)__syscall_ret(__syscall2(SYS_fchmod, fd, mode));
}
