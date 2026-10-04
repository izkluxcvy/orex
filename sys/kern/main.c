#include <stdint.h>

#include <boot_info.h>
#include <console.h>
#include <machdep.h>
#include <mm.h>
#include <mutex.h>
#include <printf.h>
#include <sched.h>
#include <thread.h>

static struct mutex  lock;
static volatile long counter;

static void *count(void *arg) {
    for (int i = 0; i < 100000; i++) {
        mutex_lock(&lock);
        long tmp = counter;
        for (volatile int spin = 0; spin < 1000; spin++) {
        }
        counter = tmp + 1;
        mutex_unlock(&lock);
    }
    return arg;
}

void kern_main(struct boot_info *boot_info) {
    struct boot_info bi = *boot_info;

    machdep_init();
    mm_init(&bi);
    console_init(&bi.fb);
    sched_init();
    machdep_init_late();

    mutex_init(&lock);
    struct thread *workers[3];
    for (long i = 0; i < 3; i++) {
        workers[i] = thread_create("count", count, (void *)i, SCHED_OTHER, 0);
    }
    for (int i = 0; i < 3; i++) {
        void *ret;
        thread_join(workers[i], &ret);
        printf("orex: worker %ld is done\n", (long)ret);
    }
    printf("orex: counter is %ld\n", counter);
}
