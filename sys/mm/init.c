#include <boot_info.h>
#include <mm.h>
#include <printf.h>

#include "pmm.h"

void mm_init(const struct boot_info *boot_info) {
    pmm_init(boot_info->memmap, boot_info->memmap_count);
    printf("mm: initialized\n");
}
