#include <stdint.h>

#include <boot_info.h>
#include <machdep.h>
#include <mm.h>
#include <printf.h>

#include "../mm/pmm.h"
#include "pmap.h"

void kern_main(struct boot_info *boot_info) {
    struct boot_info bi = *boot_info;

    machdep_init();
    mm_init(&bi);

    uintptr_t pa = pmm_alloc_page();
    uint64_t *p  = phys_to_virt(pa);
    uint64_t *q  = (uint64_t *)0xFFFF'9000'0000'0000;
    pmap_kenter((uintptr_t)q, pa, PMAP_WRITE);
    *q = 0x1234;
    printf("orex: page %lx is at %p and %p: wrote %lx, read %lx\n", pa, p, q,
           *p, *q);
    pmap_kremove((uintptr_t)q);
    *q = 1; // should cause a page fault
}
