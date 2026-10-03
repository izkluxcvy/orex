#include <stdint.h>

#include <boot_info.h>
#include <machdep.h>
#include <mm.h>
#include <printf.h>

#include "../mm/pmm.h"

void kern_main(struct boot_info *boot_info) {
    struct boot_info bi = *boot_info;

    machdep_init();
    mm_init(&bi);

    printf("orex: %lu pages free of %lu\n", pmm_free_pages(),
           pmm_total_pages());
    uintptr_t a = pmm_alloc_page();
    uintptr_t b = pmm_alloc_page();
    printf("orex: got pages %lx and %lx, %lu free\n", a, b, pmm_free_pages());
    pmm_free_page(a);
    pmm_free_page(b);
    printf("orex: gave them back, %lu free; the next is %lx\n",
           pmm_free_pages(), pmm_alloc_page());
}
