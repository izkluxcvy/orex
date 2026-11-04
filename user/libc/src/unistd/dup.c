#include <unistd.h>

#include <syscall.h>

int dup(int fd) { return (int)__syscall_ret(__syscall1(SYS_dup, fd)); }

int dup2(int fd, int nfd) {
    return (int)__syscall_ret(__syscall2(SYS_dup2, fd, nfd));
}

int dup3(int fd, int nfd, int flags) {
    return (int)__syscall_ret(__syscall3(SYS_dup3, fd, nfd, flags));
}
