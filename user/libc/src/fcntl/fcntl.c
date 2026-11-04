#include <fcntl.h>
#include <stdarg.h>

#include <pthread_impl.h>
#include <syscall.h>

int fcntl(int fd, int cmd, ...) {
    va_list ap;
    va_start(ap, cmd);
    long arg = va_arg(ap, long);
    va_end(ap);
    if (cmd == F_SETLKW || cmd == F_OFD_SETLKW) {
        return (int)__syscall_ret(__scp3(SYS_fcntl, fd, cmd, arg));
    }
    if (cmd == F_GETOWN) {
        struct f_owner_ex ex;
        long r = __syscall3(SYS_fcntl, fd, F_GETOWN_EX, (long)&ex);
        if (r < 0) {
            return (int)__syscall_ret(r);
        }
        return ex.type == F_OWNER_PGRP ? -ex.pid : ex.pid;
    }
    return (int)__syscall_ret(__scp3(SYS_fcntl, fd, cmd, arg));
}
