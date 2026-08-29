#include "gdt.c"
#include "idt.c"
#include "serial.c"
#include <printf.h>

void machdep_init() {
    serial_init();
    gdt_init();
    idt_init();
    printf("machdep: initialized\n");
}
