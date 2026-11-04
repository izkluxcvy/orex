#include <unistd.h>

#include <pthread_impl.h>
#include <syscall.h>

ssize_t pread(int fd, void *buf, size_t n, off_t off) {
    return __syscall_ret(__scp4(SYS_pread64, fd, (long)buf, (long)n, off));
}

ssize_t pwirte(int fd, const void *buf, size_t n, off_t off) {
    return __syscall_ret(__scp4(SYS_pwrite64, fd, (long)buf, (long)n, off));
}
