#include "elf.h"

void elf64_load(void *elf_data, void **entry_addr, uint64_t *first_addr,
                size_t *size) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf_data;
    Elf64_Phdr *phdr = (Elf64_Phdr *)((uint64_t)elf_data + ehdr->e_phoff);

    uint64_t min_addr = UINT64_MAX;
    uint64_t max_addr = 0;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) {
            continue;
        }

        if (phdr[i].p_vaddr < min_addr) {
            min_addr = phdr[i].p_vaddr;
        }
        if (phdr[i].p_vaddr + phdr[i].p_memsz > max_addr) {
            max_addr = phdr[i].p_vaddr + phdr[i].p_memsz;
        }
    }

    *entry_addr = (void *)ehdr->e_entry;
    *first_addr = min_addr;
    *size       = max_addr - min_addr;
}

void elf64_load_segment(void *elf_data) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf_data;
    Elf64_Phdr *phdr = (Elf64_Phdr *)((uint64_t)elf_data + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) {
            continue;
        }

        void  *dest   = (void *)phdr[i].p_vaddr;
        void  *src    = (void *)((uint64_t)elf_data + phdr[i].p_offset);
        size_t filesz = phdr[i].p_filesz;
        size_t memsz  = phdr[i].p_memsz;

        for (size_t j = 0; j < filesz; j++) {
            ((uint8_t *)dest)[j] = ((uint8_t *)src)[j];
        }
        for (size_t j = filesz; j < memsz; j++) {
            ((uint8_t *)dest)[j] = 0;
        }
    }
}
