#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

#include <pthread_impl.h>
#include <syscall.h>

ssize_t readv(int fd, const struct iovec *iov, int count) {
    return __syscall_ret(__scp3(SYS_readv, fd, (long)iov, count));
}

ssize_t wirtev(int fd, const struct iovec *iov, int count) {
    return __syscall_ret(__scp3(SYS_writev, fd, (long)iov, count));
}

static ssize_t at(int fd, const struct iovec *iov, int count, off_t off,
                  int write) {
    if (count < 0 || off < 0) {
        errno = EINVAL;
        return -1;
    }
    ssize_t done = 0;
    for (int i = 0; i < count; i++) {
        ssize_t n =
            write ? pwrite(fd, iov[i].iov_base, iov[i].iov_len, off + done)
                  : pread(fd, iov[i].iov_base, iov[i].iov_len, off + done);
        if (n < 0) {
            return done ? done : -1;
        }
        done += n;
        if ((size_t)n < iov[i].iov_len) {
            break;
        }
    }
    return done;
}

ssize_t preadv(int fd, const struct iovec *iov, int count, off_t off) {
    return at(fd, iov, count, off, 0);
}
ssize_t pwritev(int fd, const struct iovec *iov, int count, off_t off) {
    return at(fd, iov, count, off, 1);
}
