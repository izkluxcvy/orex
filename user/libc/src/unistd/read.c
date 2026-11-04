#include <unistd.h>

#include <pthread_impl.h>
#include <syscall.h>

ssize_t read(int fd, void *buf, size_t n) {
    return __syscall_ret(__scp3(SYS_read, fd, (long)buf, (long)n));
}
