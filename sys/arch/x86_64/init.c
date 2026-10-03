#include <machdep.h>
#include <printf.h>

#include "segment.h"
#include "serial.h"
#include "trap.h"

void machdep_init() {
    serial_init();
    gdt_init();
    idt_init();
    printf("machdep: initialized\n");
}
