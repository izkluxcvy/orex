#include <stdarg.h>
#include <unistd.h>

#include <syscall.h>

long syscall(long n, ...) {
    va_list ap;
    va_start(ap, n);
    long a = va_arg(ap, long);
    long b = va_arg(ap, long);
    long c = va_arg(ap, long);
    long d = va_arg(ap, long);
    long e = va_arg(ap, long);
    long f = va_arg(ap, long);
    va_end(ap);
    return __syscall_ret(__syscall6(n, a, b, c, d, e, f));
}
