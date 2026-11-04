#include <sys/wait.h>

#include <pthread_impl.h>
#include <syscall.h>

pid_t waitpid(pid_t pid, int *status, int options) {
    return (pid_t)__syscall_ret(
        __scp4(SYS_wait4, pid, (long)status, options, 0));
}

pid_t wait(int *status) { return waitpid(-1, status, 0); }

int waitid(idtype_t idtype, id_t id, siginfo_t *info, int options) {
    return (int)__syscall_ret(
        __scp5(SYS_waitid, idtype, (long)id, (long)info, options, 0));
}

pid_t wait4(pid_t pid, int *status, int options, struct rusage *ru) {
    return (pid_t)__syscall_ret(
        __scp4(SYS_wait4, pid, (long)status, options, (long)ru));
}

pid_t wait3(int *status, int options, struct rusage *ru) {
    return wait4(-1, status, options, ru);
}
