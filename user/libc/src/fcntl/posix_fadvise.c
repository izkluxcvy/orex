#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

int posix_fadvise(int fd, off_t off, off_t len, int advice) {
    (void)off;
    struct stat st;
    int saved = errno;
    if (fstat(fd, &st) != 0) {
        int err = errno;
        errno = saved;
        return err;
    }
    if (S_ISFIFO(st.st_mode)) {
        return ESPIPE;
    }
    if (advice < POSIX_FADV_NORMAL || advice > POSIX_FADV_NOREUSE || len < 0) {
        return EINVAL;
    }
    return 0;
}
