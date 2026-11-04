#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_pid_t
#define __NEED_id_t
#define __NEED_uid_t
#define __NEED_pthread_t
#define __NEED_pthread_attr_t
#define __NEED_sigset_t
#include <bits/alltypes.h>

#include <signal.h>

typedef enum { P_ALL, P_PID, P_PGID } idtype_t;

#include <bits/waitstatus.h>

pid_t wait(int *status);
int waitid(idtype_t idtype, id_t id, siginfo_t *info, int options);
pid_t waitpid(pid_t pid, int *status, int options);

struct rusage;
pid_t wait4(pid_t pid, int *status, int options, struct rusage *ru);
pid_t wait3(int *status, int options, struct rusage *ru);

#ifdef __cplusplus
}
#endif
