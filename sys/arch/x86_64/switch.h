#pragma once

#include <stdint.h>

void context_switch(uint64_t *old_rsp, uint64_t new_rsp);

uint64_t context_setup(void *stack_top, void (*entry)());
