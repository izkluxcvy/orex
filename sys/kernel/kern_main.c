#include <stdint.h>

#include <boot_info.h>
#include <kmalloc.h>
#include <printf.h>

extern void machdep_init();
extern void mm_init(const struct boot_info *boot_info);

void kern_main(struct boot_info *boot_info) {
    machdep_init();
    mm_init(boot_info);

    void *p = kmalloc(1024);
    printf("Allocated 1024 bytes at %p\n", p);
    kfree(p);

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
