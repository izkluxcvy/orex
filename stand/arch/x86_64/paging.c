#include <stdint.h>

#include <paging.h>

#define PAGE_SIZE_4K 0x1000ULL
#define PAGE_SIZE_2M 0x200000ULL

#define PTE_PRESENT   0x001ULL
#define PTE_WRITE     0x002ULL
#define PTE_HUGE      0x080ULL
#define PTE_ADDR_MASK 0x000ffffffffff000ULL

static void memset(void *dest, int c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        ((unsigned char *)dest)[i] = (unsigned char)c;
    }
}

static uint64_t pool_alloc(struct page_pool *pool) {
    if (pool->used + PAGE_SIZE_4K > pool->size) {
        return 0;
    }
    uint64_t page = pool->base + pool->used;
    pool->used += PAGE_SIZE_4K;
    memset((void *)(uintptr_t)page, 0, PAGE_SIZE_4K);
    return page;
}

static uint64_t *table_next(uint64_t *table, unsigned index,
                            struct page_pool *pool) {
    if (!(table[index] & PTE_PRESENT)) {
        uint64_t page = pool_alloc(pool);
        if (!page) {
            return nullptr;
        }
        table[index] = page | PTE_PRESENT | PTE_WRITE;
    }
    return (uint64_t *)(uintptr_t)(table[index] & PTE_ADDR_MASK);
}

static void map_2mb(uint64_t *pml4, uint64_t vaddr, uint64_t paddr,
                    struct page_pool *pool) {
    unsigned pml4_idx = (vaddr >> 39) & 0x1ff;
    unsigned pdpt_idx = (vaddr >> 30) & 0x1ff;
    unsigned pd_idx   = (vaddr >> 21) & 0x1ff;

    uint64_t *pdpt = table_next(pml4, pml4_idx, pool);
    if (!pdpt) {
        return;
    }
    uint64_t *pd = table_next(pdpt, pdpt_idx, pool);
    if (!pd) {
        return;
    }

    pd[pd_idx] =
        (paddr & ~(PAGE_SIZE_2M - 1)) | PTE_PRESENT | PTE_WRITE | PTE_HUGE;
}

static void identity_map_range(uint64_t *pml4, uint64_t start, uint64_t size,
                               struct page_pool *pool) {
    uint64_t addr = start & ~(PAGE_SIZE_2M - 1);
    uint64_t end  = (start + size + PAGE_SIZE_2M - 1) & ~(PAGE_SIZE_2M - 1);

    for (; addr < end; addr += PAGE_SIZE_2M) {
        map_2mb(pml4, addr, addr, pool);
    }
}

uint64_t paging_init(struct page_pool *pool, const struct mem_range *ranges,
                     size_t range_count, uint64_t kernel_phys_base,
                     uint64_t kernel_virt_base, uint64_t kernel_size) {
    uint64_t pml4_addr = pool_alloc(pool);
    if (!pml4_addr) {
        return 0;
    }
    uint64_t *pml4 = (uint64_t *)(uintptr_t)pml4_addr;

    for (size_t i = 0; i < range_count; i++) {
        if (ranges[i].size == 0) {
            continue;
        }
        identity_map_range(pml4, ranges[i].base, ranges[i].size, pool);
    }

    uint64_t voffset = kernel_virt_base - kernel_phys_base;
    uint64_t vstart  = kernel_virt_base & ~(PAGE_SIZE_2M - 1);
    uint64_t vend    = (kernel_virt_base + kernel_size + PAGE_SIZE_2M - 1) &
                       ~(PAGE_SIZE_2M - 1);
    for (uint64_t v = vstart; v < vend; v += PAGE_SIZE_2M) {
        map_2mb(pml4, v, v - voffset, pool);
    }

    return pml4_addr;
}
