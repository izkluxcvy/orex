#include <unistd.h>

#include <syscall.h>

pid_t getpid() { return (pid_t)__syscall0(SYS_getpid); }
