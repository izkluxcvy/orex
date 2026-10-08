#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <exec.h>
#include <kmalloc.h>
#include <printf.h>
#include <proc.h>
#include <syscall.h>
#include <thread.h>
#include <uaccess.h>
#include <vfs.h>
#include <vm.h>

static long sys_execve(const char *upath, const char *const *uargv,
                       const char *const *uenvp) {
    char *path = kmalloc(PATH_MAX);
    if (!path) {
        return -ENOMEM;
    }
    long             err = copy_str_from_user(path, upath, PATH_MAX);
    struct exec_args a;
    if (err >= 0) {
        err = exec_args_from_user(&a, uargv, uenvp);
    }
    if (err >= 0) {
        err = proc_exec(path, nullptr, &a);
        exec_args_free(&a);
    }
    kfree(path);
    return err < 0 ? err : 0;
}

#define RUSAGE_SIZE 144

static long sys_wait4(long pid, int *ustatus, long options, void *urusage) {
    if (options & ~(long)(WNOHANG | WUNTRACED | WCONTINUED) ||
        pid == INT32_MIN) {
        return -EINVAL;
    }
    int   status          = 0;
    char  ru[RUSAGE_SIZE] = {0};
    pid_t ret =
        proc_wait((pid_t)pid, &status, (int)options | WEXITED, nullptr, ru);
    if (ret > 0 && ustatus) {
        copy_to_user(ustatus, &status, sizeof(status));
    }
    if (ret >= 0 && urusage && copy_to_user(urusage, ru, sizeof(ru)) != 0) {
        return -EFAULT;
    }
    return ret;
}

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
    switch (nr) {
    case SYS_write:
        return sys_write(a0, (const char *)a1, (size_t)a2);
    case SYS_mmap:
        return sys_mmap((uintptr_t)a0, (size_t)a1, a2, a3, a4, a5);
    case SYS_mprotect:
        return sys_mprotect((uintptr_t)a0, (size_t)a1, a2);
    case SYS_munmap:
        return sys_munmap((uintptr_t)a0, (size_t)a1);
    case SYS_brk:
        return sys_brk((uintptr_t)a0);
    case SYS_getpid:
        return curproc()->pid;
    case SYS_getppid:
        return curproc()->parent ? curproc()->parent->pid : 0;
    case SYS_sched_yield:
        thread_yield();
        return 0;
    case SYS_fork:
        return proc_fork();
    case SYS_execve:
        return sys_execve((const char *)a0, (const char *const *)a1,
                          (const char *const *)a2);
    case SYS_wait4:
        return sys_wait4(a0, (int *)a1, a2, (void *)a3);
    case SYS_exit:
        proc_thread_exit(W_EXITCODE((int)a0));
    case SYS_exit_group:
        proc_exit(W_EXITCODE((int)a0));
    default:
        return -ENOSYS;
    }
}
