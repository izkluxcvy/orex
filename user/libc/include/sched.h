#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_pid_t
#define __NEED_time_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>

#define SCHED_OTHER 0
#define SCHED_FIFO  1
#define SCHED_RR    2

struct sched_param {
    int sched_priority;
};

int sched_get_priority_max(int policy);
int sched_get_priority_min(int policy);
int sched_getparam(pid_t pid, struct sched_param *param);
int sched_getscheduler(pid_t pid);
int sched_rr_get_interval(pid_t pid, struct timespec *interval);
int sched_setparam(pid_t pid, const struct sched_param *param);
int sched_setscheduler(pid_t pid, int policy, const struct sched_param *param);
int sched_yield(void);

#define CPU_SETSIZE 1024
typedef struct {
    unsigned long __bits[CPU_SETSIZE / (8 * sizeof(unsigned long))];
} cpu_set_t;

#define __CPU_WORD(i)         ((i) / (8 * sizeof(unsigned long)))
#define __CPU_BIT(i)          (1UL << ((i) % (8 * sizeof(unsigned long))))
#define CPU_ZERO_S(size, set) __builtin_memset((set), 0, (size))
#define CPU_SET_S(i, size, set)                                                \
    ((size_t)(i) < 8 * (size) ? ((set)->__bits[__CPU_WORD(i)] |= __CPU_BIT(i)) \
                              : 0)
#define CPU_CLR_S(i, size, set)                                                \
    ((size_t)(i) < 8 * (size)                                                  \
         ? ((set)->__bits[__CPU_WORD(i)] &= ~__CPU_BIT(i))                     \
         : 0)
#define CPU_ISSET_S(i, size, set)                                              \
    ((size_t)(i) < 8 * (size)                                                  \
         ? !!((set)->__bits[__CPU_WORD(i)] & __CPU_BIT(i))                     \
         : 0)
#define CPU_COUNT_S(size, set) __sched_cpucount((size), (set))
#define CPU_ZERO(set)          CPU_ZERO_S(sizeof(cpu_set_t), set)
#define CPU_SET(i, set)        CPU_SET_S(i, sizeof(cpu_set_t), set)
#define CPU_CLR(i, set)        CPU_CLR_S(i, sizeof(cpu_set_t), set)
#define CPU_ISSET(i, set)      CPU_ISSET_S(i, sizeof(cpu_set_t), set)
#define CPU_COUNT(set)         CPU_COUNT_S(sizeof(cpu_set_t), set)
#define CPU_ALLOC_SIZE(n)      ((((n) + 63) / 64) * 8)

int __sched_cpucount(unsigned long size, const cpu_set_t *set);
int sched_getaffinity(pid_t pid, unsigned long size, cpu_set_t *set);
int sched_setaffinity(pid_t pid, unsigned long size, const cpu_set_t *set);

#ifdef __cplusplus
}
#endif
