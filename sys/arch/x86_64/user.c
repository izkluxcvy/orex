#include <stdint.h>

#include <cpu.h>
#include <errno.h>
#include <memcpy.h>
#include <proc.h>
#include <sched.h>
#include <signal.h>
#include <thread.h>
#include <uaccess.h>
#include <vm.h>

#include "irq.h"
#include "msr.h"
#include "pmap.h"
#include "segment.h"
#include "switch.h"
#include "trap.h"
#include "user.h"

#define STR_(x) #x
#define STR(x)  STR_(x)

#define EFER_SCE     0x1
#define RFLAGS_IF    0x200
#define RFLAGS_TF    0x100
#define RFLAGS_DF    0x400
#define RFLAGS_USER  0x0dd5  // CF PF AF ZF SF TF DF OF
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
#define FXSAVE_MXCSR   24
#define FXSAVE_MASK    28

alignas(16) static uint8_t fpu_clean[512];
static uint32_t mxcsr_mask;

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
    memcpy(&mxcsr_mask, fpu_clean + FXSAVE_MASK, sizeof(mxcsr_mask));
    if (!mxcsr_mask) {
        mxcsr_mask = 0xffbf;
    }
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

static void fpu_sanitize(uint8_t *area) {
    uint32_t mxcsr;
    memcpy(&mxcsr, area + FXSAVE_MXCSR, sizeof(mxcsr));
    mxcsr &= mxcsr_mask;
    memcpy(area + FXSAVE_MXCSR, &mxcsr, sizeof(mxcsr));
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

struct sigcontext {
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rdi, rsi, rbp, rbx, rdx, rax, rcx, rsp, rip, eflags;
    uint16_t cs, gs, fs, ss;
    uint64_t err, trapno, oldmask, cr2;
    uint64_t fpstate;
    uint64_t reserved[8];
};

struct ucontext {
    uint64_t          flags;
    uint64_t          link;
    struct k_stack    stack;
    struct sigcontext mc;
    uint64_t          sigmask;
};

struct rt_sigframe {
    uint64_t        pretcode;
    struct ucontext uc;
    struct siginfo  info;
};

static_assert(offsetof(struct ucontext, mc) == 40);
static_assert(offsetof(struct ucontext, sigmask) == 296);

int frame_is_syscall(void *frame) {
    return ((struct trapframe *)frame)->vector == TRAP_SYSCALL;
}

long frame_retval(void *frame) {
    return (long)((struct trapframe *)frame)->rax;
}

void frame_set_retval(void *frame, long v) {
    ((struct trapframe *)frame)->rax = (uint64_t)v;
}

void frame_restart_syscall(void *frame, long nr) {
    struct trapframe *tf = frame;
    tf->rax              = (uint64_t)nr;
    tf->rip -= 2;
}

uintptr_t frame_user_sp(void *frame) {
    return frame ? ((struct trapframe *)frame)->rsp : 0;
}

static int on_stack(const struct k_stack *alt, uintptr_t sp) {
    return !(alt->flags & SS_DISABLE) && alt->size && sp >= alt->sp &&
           sp - alt->sp < alt->size;
}

int signal_frame_push(void *frame, const struct k_sigaction *act,
                      const struct siginfo *info, uint64_t blocked,
                      const struct k_stack *alt) {
    struct trapframe *tf = frame;
    uintptr_t         sp = tf->rsp - 128;
    int               on = on_stack(alt, tf->rsp);
    if ((act->flags & SA_ONSTACK) && !(alt->flags & SS_DISABLE) && alt->size &&
        !on) {
        sp = alt->sp + alt->size;
    }

    alignas(16) uint8_t fp[512];
    uintptr_t           fpaddr = (sp - sizeof(fp)) & ~(uintptr_t)63;
    uintptr_t fa = ((fpaddr - sizeof(struct rt_sigframe)) & ~(uintptr_t)15) - 8;

    irq_enable();
    int err = vm_prefault(curthread->proc->vm, fa, sp - fa, PROT_WRITE);
    (void)irq_save();
    if (err) {
        return -EFAULT;
    }
    fxsave(fp);

    struct rt_sigframe f = {
        .pretcode = act->restorer,
        .info     = *info,
        .uc = {.stack   = {.sp    = alt->sp,
                           .size  = alt->size,
                           .flags = on ? SS_ONSTACK : alt->flags & SS_DISABLE},
               .sigmask = blocked,
               .mc      = {.r8      = tf->r8,
                           .r9      = tf->r9,
                           .r10     = tf->r10,
                           .r11     = tf->r11,
                           .r12     = tf->r12,
                           .r13     = tf->r13,
                           .r14     = tf->r14,
                           .r15     = tf->r15,
                           .rdi     = tf->rdi,
                           .rsi     = tf->rsi,
                           .rbp     = tf->rbp,
                           .rbx     = tf->rbx,
                           .rdx     = tf->rdx,
                           .rax     = tf->rax,
                           .rcx     = tf->rcx,
                           .rsp     = tf->rsp,
                           .rip     = tf->rip,
                           .eflags  = tf->rflags,
                           .cs      = USER_CS,
                           .ss      = USER_DS,
                           .err     = tf->error_code,
                           .trapno  = tf->vector,
                           .oldmask = blocked,
                           .cr2     = info->addr,
                           .fpstate = fpaddr}}};
    if (copy_to_user((void *)fpaddr, fp, sizeof(fp)) != 0 ||
        copy_to_user((void *)fa, &f, sizeof(f)) != 0) {
        return -EFAULT;
    }

    fxrstor(fpu_clean);
    tf->rip = act->handler;
    tf->rsp = fa;
    tf->rdi = (uint64_t)info->si_signo;
    tf->rsi = fa + offsetof(struct rt_sigframe, info);
    tf->rdx = fa + offsetof(struct rt_sigframe, uc);
    tf->rax = 0;
    tf->rflags &= ~(uint64_t)(RFLAGS_DF | RFLAGS_TF);
    return 0;
}

int signal_frame_pop(void *frame, uint64_t *blocked, struct k_stack *alt,
                     long *rax) {
    struct trapframe *tf = frame;
    struct ucontext   uc;

    if (copy_from_user(&uc, (void *)tf->rsp, sizeof(uc)) != 0) {
        return -EFAULT;
    }
    struct sigcontext *m = &uc.mc;
    if (m->rip >= USER_TOP || m->rsp >= USER_TOP) {
        return -EFAULT;
    }
    alignas(16) uint8_t fp[512];
    if (m->fpstate) {
        if (copy_from_user(fp, (void *)m->fpstate, sizeof(fp)) != 0) {
            return -EFAULT;
        }
        fpu_sanitize(fp);
        fxrstor(fp);
    } else {
        fxrstor(fpu_clean);
    }

    *tf = (struct trapframe){
        .r15    = m->r15,
        .r14    = m->r14,
        .r13    = m->r13,
        .r12    = m->r12,
        .r11    = m->r11,
        .r10    = m->r10,
        .r9     = m->r9,
        .r8     = m->r8,
        .rdi    = m->rdi,
        .rsi    = m->rsi,
        .rbp    = m->rbp,
        .rbx    = m->rbx,
        .rdx    = m->rdx,
        .rcx    = m->rcx,
        .rax    = m->rax,
        .vector = TRAP_SYSCALL,
        .rip    = m->rip,
        .cs     = USER_CS,
        .rflags = (m->eflags & RFLAGS_USER) | RFLAGS_IF,
        .rsp    = m->rsp,
        .ss     = USER_DS,
    };
    *blocked = uc.sigmask;
    *alt     = uc.stack;
    *rax     = (long)m->rax;
    return 0;
}
