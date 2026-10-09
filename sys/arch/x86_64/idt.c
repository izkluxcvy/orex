#include <stdint.h>

#include <errno.h>
#include <fd.h>
#include <panic.h>
#include <printf.h>
#include <proc.h>
#include <syscall.h>
#include <thread.h>
#include <uaccess.h>
#include <vm.h>

#include "irq.h"
#include "pmap.h"
#include "segment.h"
#include "trap.h"

struct interrupt_descriptor {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

#define GATE_INTERRUPT 0x8E // present, ring 0, 64-bit interrupt gate
#define GATE_USER      0x60

static struct interrupt_descriptor idt[TRAP_VECTORS];

static const char *const exception_names[32] = {
    "Divide Error",
    "Debug",
    "NMI Interrupt",
    "Breakpoint",
    "Overflow",
    "BOUND Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved",
};

static void dump_frame(struct trapframe *tf) {
    printf("\n*** CPU exception: %s", exception_names[tf->vector]);
    printf(" (vector %lx, error %lx)\n", tf->vector, tf->error_code);
    printf("rip=%lx cs=%lx rflags=%lx\n", tf->rip, tf->cs, tf->rflags);
    printf("rsp=%lx ss=%lx\n", tf->rsp, tf->ss);
    printf("rax=%lx rbx=%lx rcx=%lx rdx=%lx\n", tf->rax, tf->rbx, tf->rcx,
           tf->rdx);
    printf("rsi=%lx rdi=%lx rbp=%lx\n", tf->rsi, tf->rdi, tf->rbp);
    printf("r8=%lx r9=%lx r10=%lx r11=%lx\n", tf->r8, tf->r9, tf->r10, tf->r11);
    printf("r12=%lx r13=%lx r14=%lx r15=%lx\n", tf->r12, tf->r13, tf->r14,
           tf->r15);

    if (tf->vector == 14) { // Page fault
        uint64_t cr2;
        __asm__ __volatile__("mov %0, cr2" : "=r"(cr2));
        printf("cr2=%lx\n", cr2);
    }
}

[[noreturn]] static void user_fault(struct trapframe *tf) {
    printf("trap: thread %d (%s): %s at 0x%lx\n", curthread->tid,
           curthread->name, exception_names[tf->vector], tf->rip);
    proc_exit(W_SIGNALED(tf->vector == 14 ? 11 : 4));
}

#define PF_WRITE 0x02
#define PF_FETCH 0x10

static int user_page_fault(struct trapframe *tf, uint64_t *addr) {
    uint64_t cr2;
    __asm__ __volatile__("mov %0, cr2" : "=r"(cr2));
    *addr      = cr2;
    int access = (tf->error_code & PF_WRITE)   ? PROT_WRITE
                 : (tf->error_code & PF_FETCH) ? PROT_EXEC
                                               : PROT_READ;
    irq_enable();
    int err = vm_fault(curproc()->vm, cr2, access);
    (void)irq_save();
    return err;
}

#define RFLAGS_IF 0x200

static int in_copy(struct trapframe *tf) {
    return tf->rip >= (uintptr_t)__uaccess_begin &&
           tf->rip < (uintptr_t)__uaccess_end;
}

static int kernel_user_fault(struct trapframe *tf) {
    uint64_t cr2;
    __asm__ __volatile__("mov %0, cr2" : "=r"(cr2));
    if (cr2 >= USER_TOP || !curthread->proc) {
        return 0;
    }
    if (tf->rflags & RFLAGS_IF) {
        int access = (tf->error_code & PF_WRITE) ? PROT_WRITE : PROT_READ;
        irq_enable();
        int err = vm_fault(curproc()->vm, cr2, access);
        (void)irq_save();
        if (!err) {
            return 1;
        }
    }
    if (in_copy(tf)) {
        tf->rip = (uintptr_t)__uaccess_fixup;
        return 1;
    }
    return 0;
}

void trap_handler(struct trapframe *tf) {
    if (tf->vector == TRAP_SYSCALL) {
        curthread->frame = tf;
        irq_enable();
        tf->rax = (uint64_t)syscall_dispatch(
            (long)tf->rax, (long)tf->rdi, (long)tf->rsi, (long)tf->rdx,
            (long)tf->r10, (long)tf->r8, (long)tf->r9);
        fd_return_borrowed();
        (void)irq_save();
    } else if (tf->vector == 14 && (tf->cs & 3)) {
        uint64_t addr;
        int      err = user_page_fault(tf, &addr);
        if (err) {
            printf("trap: no page for 0x%lx\n", addr);
            user_fault(tf);
        }
    } else if (tf->vector == 14 && kernel_user_fault(tf)) {
        return;
    } else if (tf->vector == 13 && in_copy(tf)) {
        tf->rip = (uintptr_t)__uaccess_fixup;
        return;
    } else if (tf->vector < 32) {
        if (!(tf->cs & 3)) {
            dump_frame(tf);
            panic("trap: unhandled CPU exception");
        } else {
            user_fault(tf);
        }
    } else {
        irq_dispatch(tf);
    }
}

static void set_gate(unsigned vector, trap_stub_t handler, uint8_t dpl) {
    uint64_t addr = (uint64_t)handler;

    idt[vector].offset_low  = addr & 0xFFFF;
    idt[vector].selector    = KERNEL_CS;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = GATE_INTERRUPT | dpl;
    idt[vector].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[vector].offset_high = (addr >> 32) & 0xFFFF'FFFF;
    idt[vector].reserved    = 0;
}

void idt_load() {
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idtr = {.limit = sizeof(idt) - 1,
                                      .base  = (uint64_t)idt};

    __asm__ __volatile__("lidt %0" : : "m"(idtr));
}

void idt_init() {
    for (unsigned i = 0; i < TRAP_VECTORS; i++) {
        set_gate(i, trap_stubs[i], i == 3 ? GATE_USER : 0);
    }
    idt_load();
}
