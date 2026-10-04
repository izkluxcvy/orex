#include <stdint.h>

#define KSTACK_SIZE (16 * 1024)

static uint8_t     kstack[KSTACK_SIZE] __attribute__((aligned(16)));
static void *const kstack_top __attribute__((used)) = kstack + KSTACK_SIZE;

struct boot_info;
extern void kern_main(struct boot_info *boot_info);

__attribute__((naked)) void _start() {
    __asm__ __volatile__("mov rsp, [rip + kstack_top]\n\t"

                         "mov r12, rdi\n\t"
                         "lea rax, [rip + cpus]\n\t"
                         "mov [rax + 16], rax\n\t"
                         "mov rdx, rax\n\t"
                         "shr rdx, 32\n\t"
                         "mov ecx, 0xC0000101\n\t"
                         "wrmsr\n\t"
                         "mov rdi, r12\n\t"

                         "call kern_main\n\t"
                         "halt_loop:\n\t"
                         "cli\n\t"
                         "hlt\n\t"
                         "jmp halt_loop");
}
