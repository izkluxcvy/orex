#include <sys/stat.h>

#include "syscall.h"

mode_t umask(mode_t mask) { return (mode_t)__syscall1(SYS_umask, mask); }
