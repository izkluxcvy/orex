#include <errno.h>

#include <syscall.h>

static __thread int errno_value;

int *__errno_location() { return &errno_value; }

long __syscall_ret(long r) {
    if ((unsigned long)r > -4096UL) {
        errno = (int)-r;
        return 1;
    }
    return r;
}
