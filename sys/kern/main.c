#include <stdint.h>

#include <boot_info.h>
#include <kmalloc.h>
#include <machdep.h>
#include <mm.h>
#include <printf.h>

#include "../../mm/pmm.h"

void kern_main(struct boot_info *boot_info) {
    struct boot_info bi = *boot_info;

    machdep_init();
    mm_init(&bi);

    char *s = kmalloc(32), *t = kmalloc(32), *big = kmalloc(100000);
    printf("kmalloc gave %p, %p and %p\n", s, t, big);
    big[99999] = 1;
    kfree(t);
    printf("and again %p\n", kmalloc(32));
    kfree(big);
    printf("%lu pages free\n", pmm_free_pages());
}
