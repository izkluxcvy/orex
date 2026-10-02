#include <stdint.h>

#include <boot_info.h>
#include <machdep.h>
#include <printf.h>

void kern_main(struct boot_info *boot_info) {
    struct boot_info *bi = boot_info;

    machdep_init();
    printf("orex: kernel started\n");
    printf("orex: kernel phys base = %p\n", (void *)bi->kernel_phys_base);
    printf("orex: kernel virt base = %p\n", (void *)bi->kernel_virt_base);
    printf("orex: kernel size = %u bytes\n", bi->kernel_size);
    printf("orex: command line = %s\n", bi->cmdline);
    for (size_t i = 0; i < bi->memmap_count; i++) {
        struct memmap_entry *e = &bi->memmap[i];
        printf("orex: memmap[%u]: base=%p, size=%u, %s\n", i, (void *)e->base,
               e->size, e->type == MEM_USABLE ? "usable" : "reserved");
    }
}
