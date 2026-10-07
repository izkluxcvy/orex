#include "ulib.h"

int main(int argc, char **argv, char **envp);

char                **environ;
static unsigned long *auxv;

unsigned long getauxval(unsigned long type) {
    for (unsigned long *p = auxv; *p != AT_NULL; p += 2) {
        if (*p == type) {
            return p[1];
        }
    }
    return 0;
}

[[noreturn]] static void start(long *sp) {
    int    argc = (int)sp[0];
    char **argv = (char **)(sp + 1);
    char **envp = argv + argc + 1;
    char **p    = envp;
    while (*p) {
        p++;
    }
    auxv    = (unsigned long *)(p + 1);
    environ = envp;
    _exit(main(argc, argv, envp));
}

__attribute__((naked)) void _start() {
    __asm__ __volatile__("xor ebp, ebp\n\t"
                         "mov rdi, rsp\n\t"
                         "call %P0"
                         :
                         : "i"(start));
}
