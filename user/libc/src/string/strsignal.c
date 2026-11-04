#include <string.h>

#include "fmt.h"

static const char *const names[] = {
    NULL,
    "Hangup",
    "Interrupt",
    "Quit",
    "Illegal instruction",
    "Trace/breakpoint trap",
    "Aborted",
    "Bus error",
    "Floating point exception",
    "Killed",
    "User defined signal 1",
    "Segmentation fault",
    "User defined signal 2",
    "Broken pipe",
    "Alarm clock",
    "Terminated",
    "Stack fault",
    "Child exited",
    "Continued",
    "Stopped (signal)",
    "Stopped",
    "Stopped (tty input)",
    "Stopped (tty output)",
    "Urgent I/O condition",
    "CPU time limit exceeded",
    "File size limit exceeded",
    "Virtual timer expired",
    "Profiling timer expired",
    "Window changed",
    "I/O possible",
    "Power failure",
    "Bad system call",
};

#define NSIG_RT_FIRST 32
#define NSIG_LAST     64

char *strsignal(int sig) {
    static __thread char buf[32];
    if (sig > 0 && (size_t)sig < sizeof(names) / sizeof(*names)) {
        return (char *)names[sig];
    }
    const char *prefix = "Unknown signal ";
    long num = sig;
    if (sig >= NSIG_RT_FIRST && sig <= NSIG_LAST) {
        prefix = "Real-time signal ";
        num = sig - NSIG_RT_FIRST;
    }
    char *end = buf + sizeof(buf) - 1;
    *end = '\0';
    char *p =
        __fmt_ulong(num < 0 ? -(unsigned long)num : (unsigned long)num, end);
    if (num < 0) {
        *--p = '-';
    }
    size_t plen = strlen(prefix);
    memmove(buf + plen, p, (size_t)(end - p) + 1);
    memcpy(buf, prefix, plen);
    return buf;
}
