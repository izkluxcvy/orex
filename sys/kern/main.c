#include <stdint.h>

#include <boot_info.h>
#include <console.h>
#include <machdep.h>
#include <mm.h>
#include <printf.h>

void kern_main(struct boot_info *boot_info) {
    struct boot_info bi = *boot_info;

    machdep_init();
    mm_init(&bi);
    console_init(&bi.fb);

    for (int i = 0; i < 60; i++) {
        printf("orex: line %d\n", i);
    }
}
