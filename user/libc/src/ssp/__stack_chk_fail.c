#include <stdint.h>
#include <unistd.h>

#include <syscall.h>
#include <tcb.h>

#define SIGABRT 6

uintptr_t __stack_chk_guard;

[[noreturn]] void __stack_chk_fail() {
    static const char msg[] = "*** stack smashing detected ***\n";
    __syscall3(SYS_write, STDERR_FILENO, (long)msg, sizeof(msg) - 1);
    __syscall2(SYS_kill, __syscall0(SYS_getpid), SIGABRT);
    _exit(127);
}
