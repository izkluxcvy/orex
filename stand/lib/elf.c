#include <stdint.h>

#include "elf.h"

static void memcpy(void *dest, const void *src, size_t n);
static void memset(void *dest, int value, size_t n);

void elf64_scan(const void *elf_data, void **entry_addr, uintptr_t *phys_base,
                uintptr_t *virt_base, size_t *size) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf_data;
    Elf64_Phdr *phdr = (Elf64_Phdr *)((uintptr_t)elf_data + ehdr->e_phoff);

    uintptr_t min_vaddr = UINTPTR_MAX;
    uintptr_t min_paddr = UINTPTR_MAX;
    uintptr_t max_vaddr = 0;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) {
            continue;
        }

        if (phdr[i].p_vaddr < min_vaddr) {
            min_vaddr = phdr[i].p_vaddr;
        }
        if (phdr[i].p_paddr < min_paddr) {
            min_paddr = phdr[i].p_paddr;
        }
        if (phdr[i].p_vaddr + phdr[i].p_memsz > max_vaddr) {
            max_vaddr = phdr[i].p_vaddr + phdr[i].p_memsz;
        }
    }

    *entry_addr = (void *)ehdr->e_entry;
    *phys_base  = min_paddr;
    *virt_base  = min_vaddr;
    *size       = max_vaddr - min_vaddr;
}

void elf64_load(const void *elf_data) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf_data;
    Elf64_Phdr *phdr = (Elf64_Phdr *)((uintptr_t)elf_data + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) {
            continue;
        }

        void  *dest   = (void *)phdr[i].p_paddr;
        void  *src    = (void *)((uintptr_t)elf_data + phdr[i].p_offset);
        size_t filesz = phdr[i].p_filesz;
        size_t memsz  = phdr[i].p_memsz;

        memcpy(dest, src, filesz);
        memset((void *)((uintptr_t)dest + filesz), 0, memsz - filesz);
    }
}

static void memcpy(void *dest, const void *src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        ((unsigned char *)dest)[i] = ((unsigned char *)src)[i];
    }
}

static void memset(void *dest, int value, size_t n) {
    for (size_t i = 0; i < n; i++) {
        ((unsigned char *)dest)[i] = (unsigned char)value;
    }
}
