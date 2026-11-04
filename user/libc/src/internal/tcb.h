#pragma once

#include <stddef.h>
#include <stdint.h>

struct tcb {
    struct tcb *self;
    uintptr_t reserved[4];
    uintptr_t stack_guard;
    uintptr_t pointer_guard;
};

static_assert(offsetof(struct tcb, stack_guard) == 0x28);

extern uintptr_t __stack_chk_guard;

void __init_tls();
