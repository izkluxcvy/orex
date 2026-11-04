#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <syscall.h>

int chdir(const char *path) {
    return (int)__syscall_ret(__syscall1(SYS_chdir, (long)path));
}

int chroot(const char *path) {
    return (int)__syscall_ret(__syscall1(SYS_chroot, (long)path));
}

int fchdir(int fd) { return (int)__syscall_ret(__syscall1(SYS_fchdir, fd)); }

char *getcwd(char *buf, size_t size) {
    if (!buf) {
        char tmp[PATH_MAX];
        return getcwd(tmp, sizeof(tmp)) ? strdup(tmp) : nullptr;
    }
    if (!size) {
        errno = EINVAL;
        return nullptr;
    }
    long r = __syscall_ret(__syscall2(SYS_getcwd, (long)buf, size));
    return r < 0 ? nullptr : buf;
}
