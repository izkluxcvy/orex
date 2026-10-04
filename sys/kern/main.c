#include <stdint.h>

#include <boot_info.h>
#include <console.h>
#include <ktime.h>
#include <machdep.h>
#include <mm.h>
#include <printf.h>

void kern_main(struct boot_info *boot_info) {
    struct boot_info bi = *boot_info;

    machdep_init();
    mm_init(&bi);
    console_init(&bi.fb);
    machdep_init_late();

    for (uint64_t last = 0;;) {
        uint64_t now = machdep_ticks() / TICK_HZ;
        if (now != last) {
            printf("orex: up for %lu seconds\n", now);
            last = now;
        }
        __asm__ __volatile__("hlt");
    }
}
