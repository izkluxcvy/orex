#include <stdint.h>

#include <boot_info.h>
#include <console.h>
#include <machdep.h>
#include <mm.h>
#include <printf.h>
#include <sched.h>
#include <thread.h>

static void *chatter(void *arg) {
    for (int i = 0; i < 5; i++) {
        printf("%s%d ", (const char *)arg, i);
        for (volatile int spin = 0; spin < 30000000; spin++) {
        }
    }
    return nullptr;
}

void kern_main(struct boot_info *boot_info) {
    struct boot_info bi = *boot_info;

    machdep_init();
    mm_init(&bi);
    console_init(&bi.fb);
    sched_init();
    machdep_init_late();

    thread_create("a", chatter, "a", SCHED_OTHER, 0);
    thread_create("b", chatter, "b", SCHED_OTHER, 0);
    thread_create("c", chatter, "c", SCHED_OTHER, 0);
    while (1) {
        __asm__ __volatile__("hlt");
    }
}
