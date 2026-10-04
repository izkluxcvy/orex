#pragma once

#include <stdint.h>

#include <waitq.h>

struct thread;

struct mutex {
    uintptr_t     word;
    struct waitq  waiters;
    struct mutex *held_next;
    int           listed;
};

#define MUTEX_WAITERS 1ul

struct cond {
    struct waitq waiters;
};

void mutex_init(struct mutex *m);
void mutex_lock(struct mutex *m);
int  mutex_trylock(struct mutex *m);
void mutex_unlock(struct mutex *m);

void mutex_pi_update(struct thread *t);

void cond_init(struct cond *c);
void cond_wait(struct cond *c, struct mutex *m);
void cond_signal(struct cond *c);
void cond_broadcast(struct cond *c);
