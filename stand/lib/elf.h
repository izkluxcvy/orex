#pragma once

#include <elf.h>

void elf64_scan(const void *elf_data, void **entry_addr, uintptr_t *phys_base,
                uintptr_t *virt_base, size_t *size);
void elf64_load(const void *elf_data, uintptr_t phys_base);
