#include <fcntl.h>
#include <unistd.h>

#include <syscall.h>

int faccessat(int dirfd, const char *path, int mode, int flags) {
    if (flags) {
        return (int)__syscall_ret(
            __syscall4(SYS_faccessat, dirfd, (long)path, mode, flags));
    } else {
        return (int)__syscall_ret(__syscall3(SYS_access, (long)path, mode, 0));
    }
}

int access(const char *path, int mode) {
    return faccessat(AT_FDCWD, path, mode, 0);
}
