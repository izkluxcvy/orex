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

struct k_sigaction;
struct k_stack;
struct siginfo;

int signal_frame_push(void *frame, const struct k_sigaction *act,
                      const struct siginfo *info, uint64_t blocked,
                      const struct k_stack *alt);
int signal_frame_pop(void *frame, uint64_t *blocked, struct k_stack *alt,
                     long *rax);

int       frame_is_syscall(void *frame);
long      frame_retval(void *frame);
void      frame_set_retval(void *frame, long v);
void      frame_restart_syscall(void *frame, long nr);
uintptr_t frame_user_sp(void *frame);
