#include <fcntl.h>
#include <unistd.h>

#include <syscall.h>

int unlinkat(int dirfd, const char *path, int flags) {
    return (int)__syscall_ret(
        __syscall3(SYS_unlinkat, dirfd, (long)path, flags));
}

int unlink(const char *path) { return unlinkat(AT_FDCWD, path, 0); }

int rmdir(const char *path) { return unlinkat(AT_FDCWD, path, AT_REMOVEDIR); }
