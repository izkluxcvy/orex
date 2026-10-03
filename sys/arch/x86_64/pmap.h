#pragma once

#include <stddef.h>
#include <stdint.h>

#include <boot_info.h>

#define PMAP_PRESENT 0x001ull
#define PMAP_WRITE   0x002ull
#define PMAP_USER    0x004ull
#define PMAP_PWT     0x008ull // write-through
#define PMAP_PCD     0x010ull // cache disable
#define PMAP_HUGE    0x080ull
#define PMAP_ADDR    0x000F'FFFF'FFFF'F000ull
#define PMAP_NX      (1ull << 63) // only once EFER.NXE is on

#define DMAP_BASE  0xFFFF'8000'0000'0000ull
#define DMAP_LIMIT 0xFFFF'C000'0000'0000ull

struct pmap {
    uintptr_t pml4;
};

extern struct pmap kernel_pmap;
extern uintptr_t   dmap_offset;

static inline void *phys_to_virt(uintptr_t pa) {
    return (void *)(pa + dmap_offset);
}

void pmap_bootstrap(const struct boot_info *bi);

int pmap_enter(struct pmap *pm, uintptr_t va, uintptr_t pa, uint64_t flags);
uint64_t pmap_pte(struct pmap *pm, uintptr_t va);
uint64_t pmap_remove(struct pmap *pm, uintptr_t va);

int  pmap_kenter(uintptr_t va, uintptr_t pa, uint64_t flags);
void pmap_kremove(uintptr_t val);

uintptr_t kvirt_to_phys(const void *va);
void     *kmap_mmio(uintptr_t pa, size_t len);
