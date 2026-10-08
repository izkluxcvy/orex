#pragma once

#include <stdint.h>

struct thread;

void syscall_init();

[[noreturn]] void usermode_enter(uint64_t rip, uint64_t rsp);

void context_set_kstack(uint64_t top);

void context_setup_fork(struct thread *child);

void context_exec(uint64_t rip, uint64_t rsp);
