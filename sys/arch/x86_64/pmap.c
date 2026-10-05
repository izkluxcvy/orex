#include <stddef.h>
#include <stdint.h>

#include <boot_info.h>
#include <kmalloc.h>

#include "../../mm/pmm.h"
#include "cpufunc.h"
#include "irq.h"
#include "msr.h"
#include "pmap.h"

#define ENTRIES     512
#define KERNEL_HALF 256
#define PAGE_2M     0x200000ull

#define EFER_NXE (1ull << 11)
#define CR0_WP   (1ULL << 16)

struct pmap     kernel_pmap;
uintptr_t       dmap_offset;
static uint64_t nx_bit;

static inline uint64_t read_cr3() {
    uint64_t cr3;
    __asm__ __volatile__("mov %0, cr3" : "=r"(cr3));
    return cr3;
}

static inline void write_cr3(uint64_t cr3) {
    __asm__ __volatile__("mov cr3, %0" : : "r"(cr3) : "memory");
}

static inline void invlpg(uintptr_t va) {
    __asm__ __volatile__("invlpg [%0]" : : "r"(va) : "memory");
}

static inline uint64_t *table_of(uint64_t entry) {
    return phys_to_virt(entry & PMAP_ADDR);
}

static inline unsigned index_at(uintptr_t va, int shift) {
    return (va >> shift) & 0x1FF;
}

static uint64_t *table_walk(uint64_t *table, unsigned index, uint64_t user) {
    if ((table[index] & (PMAP_PRESENT | PMAP_HUGE)) ==
        (PMAP_PRESENT | PMAP_HUGE)) {
        return nullptr;
    }
    if (!(table[index] & PMAP_PRESENT)) {
        uintptr_t page = pmm_alloc_page();
        if (!page) {
            return nullptr;
        }
        table[index] = page | PMAP_PRESENT | PMAP_WRITE;
    }
    table[index] |= user;
    return table_of(table[index]);
}

static uint64_t *pte_lookup(uintptr_t pml4, uintptr_t va) {
    uint64_t *table = phys_to_virt(pml4);
    for (int shift = 39; shift > 12; shift -= 9) {
        uint64_t e = table[index_at(va, shift)];
        if (!(e & PMAP_PRESENT) || (e & PMAP_HUGE)) {
            return nullptr;
        }
        table = table_of(e);
    }
    return &table[index_at(va, 12)];
}

int pmap_enter(struct pmap *pm, uintptr_t va, uintptr_t pa, uint64_t flags) {
    uint64_t  user  = flags & PMAP_USER;
    uint64_t *table = phys_to_virt(pm->pml4);

    for (int shift = 39; shift > 12; shift -= 9) {
        table = table_walk(table, index_at(va, shift), user);
        if (!table) {
            return -1;
        }
    }

    table[index_at(va, 12)] = (pa & PMAP_ADDR) | flags | PMAP_PRESENT;
    invlpg(va);
    return 0;
}

uint64_t pmap_pte(struct pmap *pm, uintptr_t va) {
    uint64_t *pte = pte_lookup(pm->pml4, va);
    return pte ? *pte : 0;
}

uint64_t pmap_remove(struct pmap *pm, uintptr_t va) {
    uint64_t *pte = pte_lookup(pm->pml4, va);
    uint64_t  old = pte ? *pte : 0;
    if (old & PMAP_PRESENT) {
        *pte = 0;
        invlpg(va);
    }
    return old;
}

uint64_t pmap_user_flags(int write, int exec) {
    return PMAP_USER | (write ? PMAP_WRITE : 0) | (exec ? 0 : nx_bit);
}

int pmap_kenter(uintptr_t va, uintptr_t pa, uint64_t flags) {
    return pmap_enter(&kernel_pmap, va, pa, flags);
}

void pmap_kremove(uintptr_t va) {
    uint64_t *pte = pte_lookup(kernel_pmap.pml4, va);
    if (pte) {
        *pte = 0;
        invlpg(va);
    }
}

uintptr_t kvirt_to_phys(const void *p) {
    uintptr_t va = (uintptr_t)p;
    if (va >= DMAP_BASE && va < DMAP_LIMIT) {
        return va - DMAP_BASE;
    }
    uint64_t *pte = pte_lookup(kernel_pmap.pml4, va);
    return pte && (*pte & PMAP_PRESENT)
               ? (*pte & PMAP_ADDR) | (va & (PAGE_SIZE - 1))
               : 0;
}

#define MMIO_VBASE  0xFFFF'FFFF'E010'0000ull
#define MMIO_VLIMIT 0xFFFF'FFFF'F000'0000ull
static uintptr_t mmio_next = MMIO_VBASE;

void *kmap_mmio(uintptr_t pa, size_t len) {
    uintptr_t first = pa & ~(PAGE_SIZE - 1);
    uintptr_t end   = (pa + len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (!len || mmio_next + (end - first) > MMIO_VLIMIT) {
        return nullptr;
    }

    uintptr_t va = mmio_next;
    for (uintptr_t off = 0; off < end - first; off += PAGE_SIZE) {
        if (pmap_kenter(va + off, first + off, PMAP_WRITE | PMAP_PCD) != 0) {
            return nullptr;
        }
    }
    mmio_next += end - first;
    return (void *)(va + (pa - first));
}

static void map_2m(uint64_t *pml4, uintptr_t va, uintptr_t pa) {
    uint64_t *pdpt = table_walk(pml4, index_at(va, 39), 0);
    uint64_t *pd   = pdpt ? table_walk(pdpt, index_at(va, 30), 0) : nullptr;
    if (pd) {
        pd[index_at(va, 21)] = pa | PMAP_PRESENT | PMAP_WRITE | PMAP_HUGE;
    }
}

static void dmap_range(uint64_t *pml4, uintptr_t base, size_t size) {
    uintptr_t end = (base + size + PAGE_2M - 1) & ~(PAGE_2M - 1);
    for (uintptr_t pa = base & ~(PAGE_2M - 1); pa < end; pa += PAGE_2M) {
        map_2m(pml4, DMAP_BASE + pa, pa);
    }
}

static void enable_nx() {
    if (cpuid(0x8000'0000).a >= 0x8000'0001 &&
        (cpuid(0x8000'0001).d & (1u << 20))) {
        wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_NXE);
        nx_bit = PMAP_NX;
    }
}

static void enable_wp() {
    uint64_t cr0;
    __asm__ __volatile__("mov %0, cr0" : "=r"(cr0));
    __asm__ __volatile__("mov cr0, %0" : : "r"(cr0 | CR0_WP));
}

void pmap_bootstrap(const struct boot_info *bi) {
    enable_nx();
    enable_wp();
    uint64_t *boot = phys_to_virt(read_cr3() & PMAP_ADDR);

    uintptr_t pml4_pa = pmm_alloc_page();
    uint64_t *pml4    = phys_to_virt(pml4_pa);

    for (unsigned i = KERNEL_HALF; i < ENTRIES - 1; i++) {
        pml4[i] = pmm_alloc_page() | PMAP_PRESENT | PMAP_WRITE;
    }
    pml4[ENTRIES - 1] = boot[ENTRIES - 1];

    for (size_t i = 0; i < bi->memmap_count; i++) {
        dmap_range(pml4, bi->memmap[i].base, bi->memmap[i].size);
    }
    dmap_range(pml4, bi->fb.base, bi->fb.size);

    kernel_pmap.pml4 = pml4_pa;
    dmap_offset      = DMAP_BASE;
    write_cr3(pml4_pa);
}

struct pmap *pmap_create() {
    struct pmap *pm = kmalloc(sizeof(*pm));
    if (!pm) {
        return nullptr;
    }
    pm->pml4 = pmm_alloc_page();
    if (!pm->pml4) {
        kfree(pm);
        return nullptr;
    }

    uint64_t *dst = phys_to_virt(pm->pml4);
    uint64_t *src = phys_to_virt(kernel_pmap.pml4);
    for (unsigned i = KERNEL_HALF; i < ENTRIES; i++) {
        dst[i] = src[i];
    }
    return pm;
}

static int walk_user(struct pmap *pm,
                     int (*fn)(uintptr_t va, uint64_t *pte, void *ctx),
                     void *ctx, int free_tables) {
    uint64_t *pml4 = phys_to_virt(pm->pml4);

    for (unsigned i = 0; i < KERNEL_HALF; i++) {
        if (!(pml4[i] & PMAP_PRESENT)) {
            continue;
        }
        uint64_t *pdpt = table_of(pml4[i]);
        for (unsigned j = 0; j < ENTRIES; j++) {
            if (!(pdpt[j] & PMAP_PRESENT)) {
                continue;
            }
            uint64_t *pd = table_of(pdpt[j]);
            for (unsigned k = 0; k < ENTRIES; k++) {
                if (!(pd[k] & PMAP_PRESENT)) {
                    continue;
                }
                uint64_t *pt = table_of(pd[k]);
                for (unsigned l = 0; l < ENTRIES; l++) {
                    if (!(pt[l] & PMAP_PRESENT)) {
                        continue;
                    }
                    uintptr_t va = ((uintptr_t)i << 39) | ((uintptr_t)j << 30) |
                                   ((uintptr_t)k << 21) | ((uintptr_t)l << 12);
                    if (fn(va, &pt[l], ctx) != 0) {
                        return -1;
                    }
                }
                if (free_tables) {
                    pmm_free_page(pd[k] & PMAP_ADDR);
                }
            }
            if (free_tables) {
                pmm_free_page(pdpt[j] & PMAP_ADDR);
            }
        }
        if (free_tables) {
            pmm_free_page(pm->pml4 & PMAP_ADDR);
            pml4[i] = 0;
        }
    }
    return 0;
}

static int free_leaf(uintptr_t va, uint64_t *pte, void *ctx) {
    (void)va, (void)ctx;
    pmm_page_unref(*pte & PMAP_ADDR);
    return 0;
}

void pmap_clear_user(struct pmap *pm) {
    walk_user(pm, free_leaf, nullptr, 1);
    uint64_t flags = irq_save();
    // If the current CR3 is the one we just cleared, reload it to flush TLB.
    if ((read_cr3() & PMAP_ADDR) == pm->pml4) {
        write_cr3(pm->pml4);
    }
    irq_restore(flags);
}

void pmap_destroy(struct pmap *pm) {
    pmap_clear_user(pm);
    pmm_free_page(pm->pml4);
    kfree(pm);
}

void pmap_activate(struct pmap *pm) {
    if ((read_cr3() & PMAP_ADDR) != pm->pml4) {
        write_cr3(pm->pml4);
    }
}
