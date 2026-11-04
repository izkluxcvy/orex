#include <unistd.h>

#include <syscall.h>

int pipe2(int fds[2], int flags) {
    return (int)__syscall_ret(__syscall2(SYS_pipe2, (long)fds, flags));
}

int pipe(int fds[2]) { return pipe2(fds, 0); }
