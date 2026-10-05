#pragma once

#include <stddef.h>
#include <stdint.h>

struct thread;
struct waitq;

#define TICK_HZ 1000
#define TICK_NS (1'000'000'000 / TICK_HZ)
#define NSEC    1000000000ll

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
