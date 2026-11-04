#include <unistd.h>

#include <syscall.h>

off_t lseek(int fd, off_t off, int whence) {
    return __syscall_ret(__syscall3(SYS_lseek, fd, off, whence));
}
