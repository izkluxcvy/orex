#include <machdep.h>
#include <printf.h>

#include "serial.h"

void machdep_init() {
    serial_init();
    printf("machdep: initialized\n");
}
