#include <stdarg.h>
#include <unistd.h>

#include <syscall.h>

int execve(const char *path, char *const argv[], char *const envp[]) {
    return (int)__syscall_ret(
        __syscall3(SYS_execve, (long)path, (long)argv, (long)envp));
}

int execv(const char *path, char *const argv[]) {
    return execve(path, argv, environ);
}

static int count(const char *arg, va_list *ap) {
    va_list c;
    va_copy(c, *ap);
    int n = 0;
    if (arg) {
        for (n = 1; va_arg(c, const char *); n++) {
        }
    }
    va_end(c);
    return n;
}

static void collect(char **argv, const char *arg, int n, va_list *ap) {
    argv[0] = (char *)arg;
    for (int i = 1; i < n; i++) {
        argv[i] = va_arg(*ap, char *);
    }
    if (arg) {
        (void)va_arg(*ap, char *);
    }
    argv[n] = nullptr;
}

int execl(const char *path, const char *arg, ...) {
    va_list ap;
    va_start(ap, arg);
    int n = count(arg, &ap);
    char *argv[n + 1];
    collect(argv, arg, n, &ap);
    va_end(ap);
    return execve(path, argv, environ);
}

int execle(const char *path, const char *arg, ...) {
    va_list ap;
    va_start(ap, arg);
    int n = count(arg, &ap);
    char *argv[n + 1];
    collect(argv, arg, n, &ap);
    char *const *envp = va_arg(ap, char *const *);
    va_end(ap);
    return execve(path, argv, envp);
}

int execlp(const char *file, const char *arg, ...) {
    va_list ap;
    va_start(ap, arg);
    int n = count(arg, &ap);
    char *argv[n + 1];
    collect(argv, arg, n, &ap);
    va_end(ap);
    return execvp(file, argv);
}
