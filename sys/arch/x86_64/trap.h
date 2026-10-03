#pragma once

#include <stdint.h>

#define TRAP_VECTORS 256

struct trapframe {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

typedef void (*trap_stub_t)();

extern const trap_stub_t trap_stubs[TRAP_VECTORS];

void trap_handler(struct trapframe *tf);

void idt_init();
void idt_load();
