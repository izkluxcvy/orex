#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <pthread_impl.h>
#include <syscall.h>

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off) {
    long r = __syscall6(SYS_mmap, (long)addr, (long)len, prot, flags, fd, off);
    return __syscall_ret(r) == -1 ? MAP_FAILED : (void *)r;
}

int mprotect(void *addr, size_t len, int prot) {
    return (int)__syscall_ret(
        __syscall3(SYS_mprotect, (long)addr, (long)len, prot));
}

int munmap(void *addr, size_t len) {
    return (int)__syscall_ret(__syscall2(SYS_munmap, (long)addr, (long)len));
}

int posix_madvise(void *addr, size_t len, int advice) {
    (void)len;
    if (((unsigned long)addr & 4095) || advice < 0 ||
        advice > POSIX_MADV_DONTNEED) {
        return EINVAL;
    }
    return 0;
}

int madvise(void *addr, size_t len, int advice) {
    int err = posix_madvise(addr, len, advice);
    if (err) {
        errno = err;
        return -1;
    }
    return 0;
}

int msync(void *addr, size_t len, int flags) {
    return (int)__syscall_ret(__scp3(SYS_msync, (long)addr, (long)len, flags));
}

#define SHM_DIR "/dev/shm/"

static int shm_path(const char *name, char *out) {
    while (*name == '/') {
        name++;
    }
    size_t n = strlen(name);
    if (!n || strchr(name, '/') || !strcmp(name, ".") || !strcmp(name, "..")) {
        errno = EINVAL;
        return -1;
    }
    if (n > NAME_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(out, SHM_DIR, sizeof(SHM_DIR) - 1);
    memcpy(out + sizeof(SHM_DIR) - 1, name, n + 1);
    return 0;
}

int shm_open(const char *name, int oflag, mode_t mode) {
    char path[sizeof(SHM_DIR) + NAME_MAX + 1];
    if (shm_path(name, path) != 0) {
        return -1;
    }
    if (oflag & ~(O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC)) {
        errno = EINVAL;
        return -1;
    }
    return open(path, oflag | O_NOFOLLOW | O_CLOEXEC, mode);
}

int shm_unlink(const char *name) {
    char path[sizeof(SHM_DIR) + NAME_MAX + 1];
    return shm_path(name, path) ? -1 : unlink(path);
}
