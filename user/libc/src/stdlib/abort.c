#include <stdlib.h>

#include <syscall.h>

#define SIGABRT     6
#define SIG_UNBLOCK 1

struct k_sigaction {
    unsigned long handler, flags, restorer, mask;
};

void abort() {
    unsigned long set = 1UL << (SIGABRT - 1);
    __syscall4(SYS_rt_sigprocmask, SIG_UNBLOCK, (long)&set, 0, 8);
    long pid = __syscall0(SYS_getpid);
    __syscall2(SYS_kill, pid, SIGABRT);

    struct k_sigaction dfl = {0};
    __syscall4(SYS_rt_sigaction, SIGABRT, (long)&dfl, 0, 8);
    __syscall2(SYS_kill, pid, SIGABRT);
    while (1) {
        __syscall1(SYS_exit_group, 127);
    }
}
