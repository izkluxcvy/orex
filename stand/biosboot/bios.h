#pragma once

#include <stdint.h>

struct bregs {
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp;
    uint16_t ds, es;
    uint32_t eflags;
    uint32_t pad[3];
};

static_assert(sizeof(struct bregs) == 48);

uint32_t bios_call(int n, struct bregs *r);
