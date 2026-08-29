#include <stdint.h>

#include "../../mm/pmm.h"
#include "pmap.h"

#define PTE_ADDR_MASK 0x000ffffffffff000ULL

static uint64_t *pml4;

static inline uint64_t read_cr3(void) {
    uint64_t cr3;
    __asm__ __volatile__("mov %0, cr3" : "=r"(cr3));
    return cr3;
}

static inline void invlpg(uintptr_t vaddr) {
    __asm__ __volatile__("invlpg [%0]" : : "r"(vaddr) : "memory");
}

void pmap_init(void) {
    pml4 = (uint64_t *)(uintptr_t)(read_cr3() & PTE_ADDR_MASK);
}

static uint64_t *table_walk(uint64_t *table, unsigned index) {
    if (!(table[index] & PMAP_PRESENT)) {
        uintptr_t page = pmm_alloc_page();
        if (page == 0) {
            return nullptr;
        }
        table[index] = page | PMAP_PRESENT | PMAP_WRITE;
    }
    return (uint64_t *)(uintptr_t)(table[index] & PTE_ADDR_MASK);
}

int pmap_kenter(uintptr_t vaddr, uintptr_t paddr, uint64_t flags) {
    unsigned pml4_idx = (vaddr >> 39) & 0x1ff;
    unsigned pdpt_idx = (vaddr >> 30) & 0x1ff;
    unsigned pd_idx   = (vaddr >> 21) & 0x1ff;
    unsigned pt_idx   = (vaddr >> 12) & 0x1ff;

    uint64_t *pdpt = table_walk(pml4, pml4_idx);
    if (!pdpt) {
        return -1;
    }
    uint64_t *pd = table_walk(pdpt, pdpt_idx);
    if (!pd) {
        return -1;
    }
    uint64_t *pt = table_walk(pd, pd_idx);
    if (!pt) {
        return -1;
    }

    pt[pt_idx] = (paddr & PTE_ADDR_MASK) | flags | PMAP_PRESENT;
    invlpg(vaddr);
    return 0;
}

void pmap_kremove(uintptr_t vaddr) {
    unsigned pml4_idx = (vaddr >> 39) & 0x1ff;
    unsigned pdpt_idx = (vaddr >> 30) & 0x1ff;
    unsigned pd_idx   = (vaddr >> 21) & 0x1ff;
    unsigned pt_idx   = (vaddr >> 12) & 0x1ff;

    if (!(pml4[pml4_idx] & PMAP_PRESENT)) {
        return;
    }
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_idx] & PTE_ADDR_MASK);
    if (!(pdpt[pdpt_idx] & PMAP_PRESENT)) {
        return;
    }
    uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & PTE_ADDR_MASK);
    if (!(pd[pd_idx] & PMAP_PRESENT)) {
        return;
    }
    uint64_t *pt = (uint64_t *)(uintptr_t)(pd[pd_idx] & PTE_ADDR_MASK);

    pt[pt_idx] = 0;
    invlpg(vaddr);
}
