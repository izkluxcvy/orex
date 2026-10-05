#pragma once

#include <stdint.h>

void syscall_init();

[[noreturn]] void usermode_enter(uint64_t rip, uint64_t rsp);

void context_set_kstack(uint64_t top);
