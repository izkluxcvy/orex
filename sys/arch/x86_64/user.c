#include <stdint.h>

#include <cpu.h>
#include <memcpy.h>
#include <thread.h>

#include "msr.h"
#include "segment.h"
#include "switch.h"
#include "trap.h"
#include "user.h"

#define STR_(x) #x
#define STR(x)  STR_(x)

#define EFER_SCE     0x1
#define RFLAGS_IF    0x200
#define SYSCALL_MASK 0x47700 // TF, IF, DF, IOPL, NT, AC

void trap_common();
void trap_return();

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

#define CR0_MP         (1ULL << 1)
#define CR0_EM         (1ULL << 2)
#define CR0_TS         (1ULL << 3)
#define CR0_NE         (1ULL << 5)
#define CR4_OSFXSR     (1ULL << 9)
#define CR4_OSXMMEXCPT (1ULL << 10)

alignas(16) static uint8_t fpu_clean[512];

static void fxsave(void *area) {
    __asm__ __volatile__("fxsave64 [%0]" : : "r"(area) : "memory");
}

static void fxrstor(const void *area) {
    __asm__ __volatile__("fxrstor64 [%0]" : : "r"(area) : "memory");
}

void fpu_init_cpu() {
    uint64_t cr0, cr4;
    __asm__ __volatile__("mov %0, cr0" : "=r"(cr0));
    __asm__ __volatile__("mov %0, cr4" : "=r"(cr4));
    cr0 = (cr0 & ~(CR0_EM | CR0_TS)) | CR0_MP | CR0_NE;
    cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;
    __asm__ __volatile__("mov cr0, %0" : : "r"(cr0));
    __asm__ __volatile__("mov cr4, %0" : : "r"(cr4));
}

void fpu_init() {
    fpu_init_cpu();
    uint32_t mxcsr = 0x1f80;
    __asm__ __volatile__("fninit\n\tldmxcsr %0" : : "m"(mxcsr));
    fxsave(fpu_clean);
}

void context_fpu_init(struct thread *t) {
    memcpy(t->fpu, fpu_clean, sizeof(t->fpu));
}

void context_fpu_switch(struct thread *prev, struct thread *next) {
    if (prev->proc) {
        fxsave(prev->fpu);
    }
    if (next->proc) {
        fxrstor(next->fpu);
    }
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

__attribute__((naked)) static void fork_trampoline() {
    __asm__ __volatile__("call sched_unlock_new_thread\n\t"
                         "cli\n\t"
                         "add rsp, 8\n\t"
                         "jmp trap_return");
}

void context_setup_fork(struct thread *child) {
    uint8_t          *top = (uint8_t *)child->stack + child->stack_size;
    struct trapframe *tf  = (struct trapframe *)top - 1;

    *tf        = *(struct trapframe *)curthread->frame;
    tf->rax    = 0;
    child->rsp = context_setup(tf, fork_trampoline);
    fxsave(child->fpu);
}

void context_exec(uint64_t rip, uint64_t rsp) {
    fxrstor(fpu_clean);
    *(struct trapframe *)curthread->frame = (struct trapframe){
        .rip    = rip,
        .cs     = USER_CS,
        .rflags = RFLAGS_IF,
        .rsp    = rsp,
        .ss     = USER_DS,
    };
}
