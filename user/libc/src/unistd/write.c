#include <unistd.h>

#include <pthread_impl.h>
#include <syscall.h>

ssize_t write(int fd, const void *buf, size_t n) {
    return __syscall_ret(__scp3(SYS_write, fd, (long)buf, (long)n));
}
