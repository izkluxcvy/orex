#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __jmp_buf_tag {
    // rbx, rbp, r12-r15, rsp, rip
    unsigned long __regs[8];
    int __saved;
    unsigned long __mask;
} jmp_buf[1], sigjmp_buf[1];

int setjmp(jmp_buf env) __attribute__((__returns_twice__));
int sigsetjmp(sigjmp_buf env, int savemask) __attribute__((__returns_twice__));
__attribute__((__noreturn__)) void longjmp(jmp_buf env, int val);
__attribute__((__noreturn__)) void siglongjmp(sigjmp_buf env, int val);
int _setjmp(jmp_buf env) __attribute__((__returns_twice__));
__attribute__((__noreturn__)) void _longjmp(jmp_buf env, int val);

#ifdef __cplusplus
}
#endif
