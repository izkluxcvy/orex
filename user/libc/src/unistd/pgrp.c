#include <unistd.h>

#include <syscall.h>

pid_t getppid() { return (pid_t)__syscall0(SYS_getppid); }

pid_t getpgid(pid_t pid) { return (pid_t)__syscall1(SYS_getpgid, pid); }

pid_t getpgrp() { return (pid_t)__syscall1(SYS_getpgid, 0); }

int setpgid(pid_t pid, pid_t pgid) {
    return (int)__syscall_ret(__syscall2(SYS_setpgid, pid, pgid));
}
