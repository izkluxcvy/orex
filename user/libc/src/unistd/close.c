#include <unistd.h>

#include <pthread_impl.h>
#include <syscall.h>

int close(int fd) { return (int)__syscall_ret(__scp1(SYS_close, fd)); }
