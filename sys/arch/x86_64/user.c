#include <stdint.h>

#include <cpu.h>

#include "msr.h"
#include "segment.h"
#include "trap.h"
#include "user.h"

#define STR_(x) #x
#define STR(x)  STR_(x)

#define EFER_SCE     0x1
#define RFLAGS_IF    0x200
#define SYSCALL_MASK 0x47700 // TF, IF, DF, IOPL, NT, AC

void trap_common();

// clang-format off
__attribute__((naked)) static void syscall_entry() {
    __asm__ __volatile__("swapgs\n\t"
                         "mov gs:[8], rsp\n\t"
                         "mov rsp, gs:[0]\n\t"
                         "push " STR(USER_DS) "\n\t"
                         "push qword ptr gs:[8]\n\t"
                         "push r11\n\t"
                         "push " STR(USER_CS) "\n\t"
                         "push rcx\n\t"
                         "swapgs\n\t"
                         "push 0\n\t"
                         "push " STR(TRAP_SYSCALL) "\n\t"
                         "jmp trap_common");
}
// clang-format on

void syscall_init() {
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_SCE);
    wrmsr(MSR_STAR,
          ((uint64_t)(USER_DS - 8 - 3) << 48) | ((uint64_t)KERNEL_CS << 32));
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSR_FMASK, SYSCALL_MASK);

    wrmsr(MSR_KERNEL_GS_BASE, 0);
}

void context_set_kstack(uint64_t top) {
    tss_set_rsp0(top);
    curcpu()->kernel_rsp = top;
}

void usermode_enter(uint64_t rip, uint64_t rsp) {
    __asm__ __volatile__("cli\n\t"
                         "push %2\n\t"
                         "push %1\n\t"
                         "push %3\n\t"
                         "push %4\n\t"
                         "push %0\n\t"
                         "swapgs\n\t"
                         "iretq"
                         :
                         : "r"(rip), "r"(rsp), "i"(USER_DS), "i"(RFLAGS_IF),
                           "i"(USER_CS)
                         : "memory");
    __builtin_unreachable();
}
