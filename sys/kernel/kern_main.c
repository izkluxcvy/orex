#include <stdint.h>

extern void machdep_init();

void kern_main() {
    machdep_init();
    // divide by zero exception
    __asm__ __volatile__("xor eax, eax\n\t"
                         "xor edx, edx\n\t"
                         "xor ecx, ecx\n\t"
                         "div ecx" ::
                             : "eax", "ecx", "edx");
    while (1) {
        __asm__ __volatile__("hlt");
    }
}
