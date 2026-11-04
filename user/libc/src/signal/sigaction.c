#include <errno.h>
#include <signal.h>

#include "syscall.h"

#define SA_RESTORER 0x04000000

struct k_sigaction {
    void (*handler)(int);
    unsigned long flags;
    void (*restorer)(void);
    unsigned long mask;
};

__attribute__((naked)) static void restore_rt(void) {
    __asm__ __volatile__("mov eax, 15\n\t"
                         "syscall");
}

int __sigaction_internal(int sig, const struct sigaction *__restrict act,
                         struct sigaction *__restrict old);

int sigaction(int sig, const struct sigaction *__restrict act,
              struct sigaction *__restrict old) {
    if (sig == 32 || sig == 33) {
        errno = EINVAL;
        return -1;
    }
    return __sigaction_internal(sig, act, old);
}

int __sigaction_internal(int sig, const struct sigaction *__restrict act,
                         struct sigaction *__restrict old) {
    struct k_sigaction k, ko;
    if (act) {
        k = (struct k_sigaction){.handler = act->sa_handler,
                                 .flags = (unsigned)act->sa_flags | SA_RESTORER,
                                 .restorer = restore_rt,
                                 .mask = act->sa_mask.__bits[0]};
    }
    long r = __syscall4(SYS_rt_sigaction, sig, act ? (long)&k : 0,
                        old ? (long)&ko : 0, 8);
    if (r) {
        return (int)__syscall_ret(r);
    }
    if (old) {
        old->sa_handler = ko.handler;
        old->sa_flags = (int)(ko.flags & ~(unsigned long)SA_RESTORER);
        old->sa_mask.__bits[0] = ko.mask;
    }
    return 0;
}

void (*signal(int sig, void (*handler)(int)))(int) {
    struct sigaction sa = {.sa_handler = handler, .sa_flags = SA_RESTART}, old;
    return sigaction(sig, &sa, &old) ? SIG_ERR : old.sa_handler;
}
