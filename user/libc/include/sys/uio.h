#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_off_t
#include <bits/alltypes.h>

struct iovec {
    void *iov_base;
    size_t iov_len;
};

ssize_t readv(int fd, const struct iovec *iov, int count);
ssize_t writev(int fd, const struct iovec *iov, int count);
// Not POSIX: readv and writev at an offset, as glibc and the BSDs have them.
ssize_t preadv(int fd, const struct iovec *iov, int count, off_t off);
ssize_t pwritev(int fd, const struct iovec *iov, int count, off_t off);

#ifdef __cplusplus
}
#endif
