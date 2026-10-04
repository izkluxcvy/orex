#pragma once

#include <stdint.h>

#define SCHED_OTHER 0
#define SCHED_FIFO  1
#define SCHED_RR    2

#define SCHED_PRIO_MIN    0
#define SCHED_PRIO_MAX    99
#define SCHED_PRIO_LEVELS (SCHED_PRIO_MAX + 1)

#define SCHED_QUANTUM 20

struct spinlock;
struct thread;

int sched_get_priority_min(int policy);
int sched_get_priority_max(int policy);

extern struct spinlock sched_lock;

void sched_init();
void sched_enqueue(struct thread *t);
void schedule();

void sched_unlock_new_thread();

void sched_tick();
void sched_preempt();
