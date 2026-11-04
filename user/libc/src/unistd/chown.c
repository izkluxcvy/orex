#include <fcntl.h>
#include <unistd.h>

#include <syscall.h>

int fchownat(int dirfd, const char *path, uid_t uid, gid_t gid, int flags) {
    return (int)__syscall_ret(
        __syscall5(SYS_fchownat, dirfd, (long)path, uid, gid, flags));
}

int chown(const char *path, uid_t uid, gid_t gid) {
    return fchownat(AT_FDCWD, path, uid, gid, 0);
}

int lchown(const char *path, uid_t uid, gid_t gid) {
    return fchownat(AT_FDCWD, path, uid, gid, AT_SYMLINK_NOFOLLOW);
}

int fchown(int fd, uid_t uid, gid_t gid) {
    return (int)__syscall_ret(__syscall3(SYS_fchown, fd, uid, gid));
}
