#include <stdarg.h>
#include <stdint.h>

#include <panic.h>
#include <printf.h>

[[noreturn]] static void hang() {
    while (1) {
        __asm__ __volatile__("cli\n\thlt");
    }
}

[[noreturn]] void panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("\npanic: ");
    vprintf(fmt, args);
    va_end(args);
    printf("\n");

    hang();
}
