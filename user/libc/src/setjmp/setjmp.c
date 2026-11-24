#include <setjmp.h>
#include <signal.h>
#include <stddef.h>

__attribute__((naked)) static void save() {
    __asm__ __volatile__("mov [rdi], rbx\n\t"
                         "mov [rdi + 8], rbp\n\t"
                         "mov [rdi + 16], r12\n\t"
                         "mov [rdi + 24], r13\n\t"
                         "mov [rdi + 32], r14\n\t"
                         "mov [rdi + 40], r15\n\t"
                         "lea rdx, [rsp + 16]\n\t"
                         "mov [rdi + 48], rdx\n\t"
                         "mov rdx, [rsp + 8]\n\t"
                         "mov [rdi + 56], rdx\n\t"
                         "xor eax, eax\n\t"
                         "ret");
}

__attribute__((naked)) int setjmp(jmp_buf env) {
    __asm__ __volatile__("mov dword ptr [rdi + 64], 0\n\t"
                         "call %P0\n\t"
                         "ret"
                         :
                         : "i"(save));
}

__attribute__((naked)) int sigsetjmp(sigjmp_buf env, int savemask) {
    __asm__ __volatile__("mov [rdi + 64], esi\n\t"
                         "test esi, esi\n\t"
                         "jz 1f\n\t"
                         "push rdi\n\t"
                         "lea rdx, [rdi + 72]\n\t"
                         "xor esi, esi\n\t"
                         "xor edi, edi\n\t"
                         "mov r10d, 8\n\t"
                         "mov eax, 14\n\t"
                         "syscall\n\t"
                         "pop rdi\n\t"
                         "1:\n\t"
                         "call %P0\n\t"
                         "ret"
                         :
                         : "i"(save));
}

__attribute__((naked)) void longjmp(jmp_buf env, int val) {
    __asm__ __volatile__("mov eax, esi\n\t"
                         "test eax, eax\n\t"
                         "jnz 1f\n\t"
                         "inc eax\n\t"
                         "1:\n\t"
                         "mov rbx, [rdi]\n\t"
                         "mov rbp, [rdi + 8]\n\t"
                         "mov r12, [rdi + 16]\n\t"
                         "mov r13, [rdi + 24]\n\t"
                         "mov r14, [rdi + 32]\n\t"
                         "mov r15, [rdi + 40]\n\t"
                         "mov rsp, [rdi + 48]\n\t"
                         "jmp qword ptr [rdi + 56]");
}

void siglongjmp(sigjmp_buf env, int val) {
    if (env->__saved) {
        sigset_t mask = {{env->__mask}};
        sigprocmask(SIG_SETMASK, &mask, nullptr);
    }
    longjmp(env, val);
}

int _setjmp(jmp_buf env) __attribute__((alias("setjmp")));
void _longjmp(jmp_buf env, int val) __attribute__((alias("longjmp")));
