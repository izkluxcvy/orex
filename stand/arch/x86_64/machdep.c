#include <stdint.h>

#include <paging.h>

extern uintptr_t         kernel_phys_base;
extern uintptr_t         kernel_virt_base;
extern size_t            kernel_size;
extern struct mem_range *mem_ranges;
extern size_t            mem_range_count;
extern struct page_pool  paging_pool;

uint64_t paging_init(struct page_pool *pool, const struct mem_range *ranges,
                     size_t range_count, uint64_t kernel_phys_base,
                     uint64_t kernel_virt_base, uint64_t kernel_size);

void machdep_init() {
    paging_init(&paging_pool, mem_ranges, mem_range_count, kernel_phys_base,
                kernel_virt_base, kernel_size);

    __asm__ __volatile__("mov %0, %%cr3" ::"r"(paging_pool.base) : "memory");
}
