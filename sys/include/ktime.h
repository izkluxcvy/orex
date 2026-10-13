#pragma once

#include <stddef.h>
#include <stdint.h>

struct proc;
struct thread;
struct waitq;

#define TICK_HZ 1000
#define TICK_NS (1'000'000'000 / TICK_HZ)
#define NSEC    1000000000ll

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3
#define CLOCK_MONOTONIC_RAW      4
#define CLOCK_REALTIME_COARSE    5
#define CLOCK_MONOTONIC_COARSE   6
#define CLOCK_BOOTTIME           7

#define TIMER_ABSTIME 1
#define TIMER_MAX     32

uint64_t machdep_cycles();
uint64_t machdep_cycles_hz();
uint64_t rtc_read();

void     time_init();
void     time_tick(int user);
uint64_t time_mono();
int64_t  time_real();

int sleep_until(struct waitq *wq, uint64_t deadline);

void     time_switch(struct thread *prev, struct thread *next);
uint64_t time_thread_cpu(struct thread *t);

void time_fork(struct proc *child);
void time_exec(struct proc *p);
void time_exit(struct proc *p);
void time_reap(struct proc *parent, struct proc *child);

struct k_timespec {
    int64_t sec, nsec;
};

long sys_clock_gettime(long clock, struct k_timespec *uts);
long sys_clock_settime(long clock, const struct k_timespec *uts);
long sys_clock_getres(long clock, struct k_timespec *uts);
long sys_nanosleep(const struct k_timespec *ureq, struct k_timespec *urem);
long sys_clock_nanosleep(long clock, long flags, const struct k_timespec *ureq,
                         struct k_timespec *urem);
long sys_setitimer(long which, const void *unew, void *uold);
long sys_getitimer(long which, void *ucur);
long sys_timer_create(long clock, const void *usev, int *uid);
long sys_timer_settime(long id, long flags, const void *unew, void *uold);
long sys_timer_gettime(long id, void *ucur);
long sys_timer_getoverrun(long id);
long sys_timer_delete(long id);
long sys_times(void *utms);
long sys_getrusage(long who, void *uru);
void time_rusage(struct proc *p, int which, void *out); // a struct rusage

struct siginfo;
void time_timer_taken(struct proc *p, struct siginfo *info);
