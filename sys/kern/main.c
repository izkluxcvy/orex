#include <stdint.h>

#include <boot_info.h>
#include <console.h>
#include <ktime.h>
#include <machdep.h>
#include <mm.h>
#include <printf.h>
#include <sched.h>
#include <spinlock.h>
#include <thread.h>

void kern_main(struct boot_info *boot_info) {
    struct boot_info bi = *boot_info;

    machdep_init();
    mm_init(&bi);
    console_init(&bi.fb);
    sched_init();
    machdep_init_late();
    time_init();

    for (int i = 0; i < 10; i++) {
        uint64_t flags = spin_lock_irqsave(&sched_lock);
        sleep_until(nullptr, time_mono() + NSEC);
        spin_unlock_irqrestore(&sched_lock, flags);
        printf("orex: %lu ms since boot, of which this thread ran %lu ms\n",
               (long)(time_mono() / 1000000),
               (long)(time_thread_cpu(curthread) / 1000000));
    }
}
