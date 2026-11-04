#include <errno.h>
#include <unistd.h>

#include <syscall.h>

#define TCGETS 0x5401

int isatty(int fd) {
    unsigned char termios[64];
    long r = __syscall3(SYS_ioctl, fd, TCGETS, (long)termios);
    if (r < 0) {
        errno = r == -EBADF ? EBADF : ENOTTY;
        return 0;
    }
    return 1;
}
