#include <unistd.h>

#include <pthread_impl.h>
#include <syscall.h>

int fsync(int fd) { return (int)__syscall_ret(__scp1(SYS_fsync, fd)); }

int fdatasync(int fd) { return (int)__syscall_ret(__scp1(SYS_fdatasync, fd)); }

void sync() { __syscall0(SYS_sync); }
