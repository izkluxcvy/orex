#include <stdint.h>

#include "switch.h"

__attribute__((naked)) void context_switch(uint64_t *old_rsp,
                                           uint64_t  new_rsp) {
    __asm__ __volatile__("push rbp\n\t"
                         "push rbx\n\t"
                         "push r12\n\t"
                         "push r13\n\t"
                         "push r14\n\t"
                         "push r15\n\t"

                         "mov [rdi], rsp\n\t"
                         "mov rsp, rsi\n\t"

                         "pop r15\n\t"
                         "pop r14\n\t"
                         "pop r13\n\t"
                         "pop r12\n\t"
                         "pop rbx\n\t"
                         "pop rbp\n\t"
                         "ret");
}

uint64_t context_setup(void *stack_top, void (*entry)()) {
    uintptr_t top = (uintptr_t)stack_top & ~(uintptr_t)0xF;

    uint64_t *frame = (uint64_t *)(top - 16);
    frame[0]        = (uint64_t)entry;

    uint64_t *regs = frame - 6;
    for (int i = 0; i < 6; i++) {
        regs[i] = 0;
    }

    return (uint64_t)regs;
}
