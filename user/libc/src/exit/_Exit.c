#include <stdlib.h>
#include <unistd.h>

#include <syscall.h>

void _Exit(int status) {
    while (1) {
        __syscall1(SYS_exit, status);
    }
}

void _exit(int status) { _Exit(status); }
