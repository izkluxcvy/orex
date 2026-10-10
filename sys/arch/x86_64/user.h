#pragma once

#include <stdint.h>

struct thread;

void syscall_init();

[[noreturn]] void usermode_enter(uint64_t rip, uint64_t rsp);

void context_set_kstack(uint64_t top);

void fpu_init();
void fpu_init_cpu();
void context_fpu_init(struct thread *t);
void context_fpu_switch(struct thread *prev, struct thread *next);

void context_setup_fork(struct thread *child);

void context_exec(uint64_t rip, uint64_t rsp);
