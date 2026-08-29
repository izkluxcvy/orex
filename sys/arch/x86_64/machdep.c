#include "gdt.c"
#include "idt.c"
#include "serial.c"

void machdep_init() {
    serial_init();
    gdt_init();
    idt_init();
    serial_puts("machdep: serial, gdt, idt initialized\r\n");
}
