#pragma once

#include <stdint.h>

#define PMAP_PRESENT 0x001ULL
#define PMAP_WRITE   0x002ULL

void pmap_init(void);
int  pmap_kenter(uintptr_t vaddr, uintptr_t paddr, uint64_t flags);
void pmap_kremove(uintptr_t vaddr);
