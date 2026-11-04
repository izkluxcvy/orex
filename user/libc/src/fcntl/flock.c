#include <fcntl.h>
#include <sys/file.h>

int flock(int fd, int op) {
    struct flock fl = {.l_type = op & LOCK_UN   ? F_UNLCK
                                 : op & LOCK_EX ? F_WRLCK
                                                : F_RDLCK,
                       .l_whence = SEEK_SET};
    return fcntl(fd, op & LOCK_NB ? F_OFD_SETLK : F_OFD_SETLKW, &fl);
}
