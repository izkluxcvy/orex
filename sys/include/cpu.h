#pragma once

#include <stddef.h>
#include <stdint.h>

struct thread;

#define MAX_CPUS 32

struct cpu {
    uint64_t       kernel_rsp;
    uint64_t       user_rsp;
    struct cpu    *self;
    struct thread *thread;
    int            id;
    uint32_t       apic_id;
    struct thread *idle;
    volatile int   need_resched;
    int            nlocks;
    const char    *locks[8];
};

static_assert(offsetof(struct cpu, kernel_rsp) == 0);
static_assert(offsetof(struct cpu, user_rsp) == 8);
static_assert(offsetof(struct cpu, self) == 16);
static_assert(offsetof(struct cpu, thread) == 24);

extern struct cpu cpus[MAX_CPUS];
extern int        ncpu;

// TODO: avoid arch depends
static inline struct cpu *curcpu() {
    struct cpu *c;
    __asm__ __volatile__("mov %0, gs:[16]" : "=r"(c));
    return c;
}

static inline struct thread *curthread_load() {
    struct thread *t;
    __asm__ __volatile__("mov %0, gs:[24]" : "=r"(t));
    return t;
}

#define curthread (curthread_load())
