#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <printf.h>
#include <syscall.h>
#include <thread.h>
#include <uaccess.h>

static long sys_write(long fd, const char *ubuf, size_t count) {
    (void)fd;
    char   buf[128];
    size_t done = 0;
    while (done < count) {
        size_t n = count - done < sizeof(buf) ? count - done : sizeof(buf);
        if (copy_from_user(buf, ubuf + done, n) != 0) {
            return done ? (long)done : -EFAULT;
        }
        printf_write(buf, n);
        done += n;
    }
    return (long)done;
}

long syscall_dispatch(long nr, long a0, long a1, long a2, long a3, long a4,
                      long a5) {
    (void)a3, (void)a4, (void)a5;
    switch (nr) {
    case SYS_write:
        return sys_write(a0, (const char *)a1, (size_t)a2);
    case SYS_exit:
    case SYS_exit_group:
        thread_exit(nullptr);
    default:
        return -ENOSYS;
    }
}
