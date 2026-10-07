#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <kmalloc.h>
#include <memcpy.h>
#include <memset.h>
#include <mutex.h>
#include <proc.h>
#include <vfs.h>
#include <vm.h>

#include "../mm/pmm.h"
#include "pmap.h"

#define MMAP_TOP (USER_STACK_TOP - USER_STACK_MAX - MMAP_GAP)

static uintptr_t page_up(uintptr_t v) {
    return (v + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static uint64_t pte_flags(int prot) {
    if (prot == PROT_NONE) {
        return 0;
    }
    return pmap_user_flags(prot & PROT_WRITE, prot & PROT_EXEC);
}

static void area_free(struct vm_area *a) {
    if (a->vn) {
        vnode_put(a->vn);
    }
    kfree(a);
}

static void area_dup(struct vm_area *a) {
    if (a->vn) {
        vnode_ref(a->vn);
    }
}

static struct vm_area *area_find(struct vmspace *vm, uintptr_t va) {
    for (struct vm_area *a = vm->areas; a && a->start <= va; a = a->next) {
        if (va < a->end) {
            return a;
        }
    }
    return nullptr;
}

static int range_free(struct vmspace *vm, uintptr_t start, uintptr_t end) {
    for (struct vm_area *a = vm->areas; a && a->start < end; a = a->next) {
        if (a->end > start) {
            return 0;
        }
    }
    return 1;
}

static uintptr_t find_free(struct vmspace *vm, size_t len) {
    uintptr_t       best = 0, lo = USER_BASE, top = vm->mmap_top;
    struct vm_area *a = vm->areas;
    while (lo < top) {
        uintptr_t hi = a && a->start < top ? a->start : top;
        if (hi - lo >= len) {
            best = hi - len;
        }
        if (!a) {
            break;
        }
        lo = a->end;
        a  = a->next;
    }
    return best;
}

static struct vm_area *split(struct vm_area *a, uintptr_t at) {
    struct vm_area *b = kmalloc(sizeof(*b));
    if (!b) {
        return nullptr;
    }
    *b       = *a;
    b->start = at;
    b->off += at - a->start;
    area_dup(b);
    a->end  = at;
    a->next = b;
    return b;
}

static void free_pages(struct vmspace *vm, uintptr_t start, uintptr_t end) {
    for (uintptr_t va = start; va < end; va += PAGE_SIZE) {
        uint64_t pte = pmap_remove(vm->pmap, va);
        if (pte & PMAP_PRESENT) {
            pmm_page_unref(pte & PMAP_ADDR);
        }
    }
}

struct vmspace *vm_create() {
    struct vmspace *vm = kmalloc(sizeof(*vm));
    if (!vm) {
        return nullptr;
    }
    *vm = (struct vmspace){.stack_top = USER_STACK_TOP, .mmap_top = MMAP_TOP};
    mutex_init(&vm->lock);
    vm->pmap = pmap_create();
    if (!vm->pmap) {
        kfree(vm);
        return nullptr;
    }
    return vm;
}

void vm_clear(struct vmspace *vm) {
    pmap_clear_user(vm->pmap);
    while (vm->areas) {
        struct vm_area *a = vm->areas;
        vm->areas         = a->next;
        area_free(a);
    }
    vm->brk_base = vm->brk = 0;
}

void vm_destroy(struct vmspace *vm) {
    vm_clear(vm);
    pmap_destroy(vm->pmap);
    kfree(vm);
}

static int mergeable(struct vm_area *a, int prot) {
    return a && !a->vn && a->prot == prot;
}

int vm_map(struct vmspace *vm, uintptr_t start, uintptr_t end, int prot,
           struct vnode *vn, uint64_t off, uintptr_t file_end) {
    if (!range_free(vm, start, end)) {
        return -EINVAL;
    }
    struct vm_area **pp = &vm->areas, *prev = nullptr;
    while (*pp && (*pp)->start < start) {
        prev = *pp;
        pp   = &(*pp)->next;
    }
    struct vm_area *next = *pp;

    if (!vn && mergeable(prev, prot) && prev->end == start) {
        prev->end = end;
        if (mergeable(next, prot) && next->start == end) {
            prev->end  = next->end;
            prev->next = next->next;
            kfree(next);
        }
        return 0;
    }
    if (!vn && mergeable(next, prot) && next->start == end) {
        next->start = start;
        return 0;
    }

    struct vm_area *a = kmalloc(sizeof(*a));
    if (!a) {
        return -ENOMEM;
    }
    *a  = (struct vm_area){.start    = start,
                           .end      = end,
                           .prot     = prot,
                           .vn       = vn ? vnode_ref(vn) : nullptr,
                           .off      = off,
                           .file_end = file_end,
                           .next     = next};
    *pp = a;
    return 0;
}

static int carve(struct vmspace *vm, uintptr_t start, uintptr_t end,
                 struct vm_area ***first) {
    struct vm_area **pp = &vm->areas;
    while (*pp && (*pp)->end <= start) {
        pp = &(*pp)->next;
    }
    if (*pp && (*pp)->start < start) {
        if (!split(*pp, start)) {
            return -ENOMEM;
        }
        pp = &(*pp)->next;
    }
    *first = pp;
    for (struct vm_area *a = *pp; a && a->start < end; a = a->next) {
        if (a->end > end && !split(a, end)) {
            return -ENOMEM;
        }
    }
    return 0;
}

int vm_unmap(struct vmspace *vm, uintptr_t start, uintptr_t end) {
    struct vm_area **pp;
    int              err = carve(vm, start, end, &pp);
    if (err) {
        return err;
    }
    while (*pp && (*pp)->start < end) {
        struct vm_area *a = *pp;
        *pp               = a->next;
        free_pages(vm, a->start, a->end);
        area_free(a);
    }
    return 0;
}

static int vm_protect(struct vmspace *vm, uintptr_t start, uintptr_t end,
                      int prot) {
    uintptr_t covered = start;
    for (struct vm_area *a = vm->areas; a && covered < end; a = a->next) {
        if (a->end > covered && a->start <= covered) {
            covered = a->end;
        }
    }
    if (covered < end) {
        return -ENOMEM;
    }

    struct vm_area **pp;
    int              err = carve(vm, start, end, &pp);
    if (err) {
        return err;
    }
    for (struct vm_area *a = *pp; a && a->start < end; a = a->next) {
        a->prot = prot;
        for (uintptr_t va = a->start; va < a->end; va += PAGE_SIZE) {
            pmap_protect(vm->pmap, va, pte_flags(prot));
        }
    }
    return 0;
}

static int fault_locked(struct vmspace *vm, uintptr_t va, int access) {
    struct vm_area *a = area_find(vm, va);
    if (!a) {
        return -EFAULT;
    }
    if (!(a->prot & access)) {
        return -EACCES;
    }
    uintptr_t page  = va & ~(PAGE_SIZE - 1);
    uint64_t  flags = pte_flags(a->prot);
    uint64_t  pte   = pmap_pte(vm->pmap, page);
    if (pte & PMAP_PRESENT) {
        pmap_protect(vm->pmap, page, flags);
        return 0;
    }

    uintptr_t pa = pmm_alloc_page();
    if (!pa) {
        return -ENOMEM;
    }
    uint8_t *kva = phys_to_virt(pa);
    if (a->vn && page < a->file_end) {
        size_t n =
            a->file_end - page < PAGE_SIZE ? a->file_end - page : PAGE_SIZE;
        if (vnode_read(a->vn, kva, n, a->off + (page - a->start)) < 0) {
            pmm_free_page(pa);
            return -EIO;
        }
    }
    if (pmap_enter(vm->pmap, page, pa, flags) != 0) {
        pmm_free_page(pa);
        return -ENOMEM;
    }
    return 0;
}

static int fault_once(struct vmspace *vm, uintptr_t va, int access) {
    mutex_lock(&vm->lock);
    int err = fault_locked(vm, va, access);
    mutex_unlock(&vm->lock);
    return err;
}

int vm_fault(struct vmspace *vm, uintptr_t va, int access) {
    if (!vm) {
        return -EFAULT;
    }
    return fault_once(vm, va, access);
}

int vm_write(struct vmspace *vm, uintptr_t va, const void *src, size_t len) {
    const uint8_t *s = src;
    while (len) {
        size_t in    = va & (PAGE_SIZE - 1);
        size_t chunk = PAGE_SIZE - in < len ? PAGE_SIZE - in : len;
        int    err   = vm_fault(vm, va, PROT_WRITE);
        if (err) {
            return err;
        }
        uintptr_t pa = pmap_pte(vm->pmap, va) & PMAP_ADDR;
        memcpy((uint8_t *)phys_to_virt(pa) + in, s, chunk);
        va += chunk, s += chunk, len -= chunk;
    }
    return 0;
}

static int user_range(uintptr_t addr, size_t size) {
    return !(addr & (PAGE_SIZE - 1)) && addr >= USER_BASE && size &&
           size <= USER_TOP - addr;
}

static long do_mmap(uintptr_t addr, size_t len, long prot, long flags, long fd,
                    long off) {
    struct proc    *p    = curproc();
    struct vmspace *vm   = p->vm;
    long            type = flags & (MAP_SHARED | MAP_PRIVATE);
    size_t          size = page_up(len);
    if (!len || (prot & ~7L) || (type != MAP_SHARED && type != MAP_PRIVATE) ||
        (off & (PAGE_SIZE - 1))) {
        return -EINVAL;
    }
    if (size < len || size > USER_TOP) {
        return -ENOMEM;
    }
    struct vnode *vn = nullptr;
    if (!(flags & MAP_ANONYMOUS) || type == MAP_SHARED) {
        (void)fd;
        return -ENODEV;
    }

    if (flags & MAP_FIXED) {
        if (!user_range(addr, size)) {
            return -EINVAL;
        }
        int err = vm_unmap(vm, addr, addr + size);
        if (err) {
            return err;
        }
    } else if (!user_range(addr, size) || !range_free(vm, addr, addr + size)) {
        if (!(addr = find_free(vm, size))) {
            return -ENOMEM;
        }
    }
    int err = vm_map(vm, addr, addr + size, (int)prot, vn, (uint64_t)off,
                     addr + size);
    return err ? err : (long)addr;
}

static long do_brk(uintptr_t addr) {
    struct vmspace *vm = curproc()->vm;
    if (addr < vm->brk_base || addr > vm->mmap_top) {
        return (long)vm->brk;
    }
    uintptr_t old = page_up(vm->brk);
    uintptr_t new = page_up(addr);
    if (new > old) {
        if (!range_free(vm, old, new) ||
            vm_map(vm, old, new, PROT_READ | PROT_WRITE, nullptr, 0, 0)) {
            return (long)vm->brk;
        }
    } else if (new < old && vm_unmap(vm, new, old) != 0) {
        return (long)vm->brk;
    }
    vm->brk = addr;
    return (long)vm->brk;
}

long sys_mmap(uintptr_t addr, size_t len, long prot, long flags, long fd,
              long off) {
    struct vmspace *vm = curproc()->vm;
    mutex_lock(&vm->lock);
    long r = do_mmap(addr, len, prot, flags, fd, off);
    mutex_unlock(&vm->lock);
    return r;
}

long sys_munmap(uintptr_t addr, size_t len) {
    struct vmspace *vm = curproc()->vm;
    mutex_lock(&vm->lock);
    long r = vm_unmap(vm, addr, addr + len);
    mutex_unlock(&vm->lock);
    return r;
}

long sys_mprotect(uintptr_t addr, size_t len, long prot) {
    struct vmspace *vm = curproc()->vm;
    mutex_lock(&vm->lock);
    long r = vm_protect(vm, addr, addr + len, (int)prot);
    mutex_unlock(&vm->lock);
    return r;
}

long sys_brk(uintptr_t addr) {
    struct vmspace *vm = curproc()->vm;
    mutex_lock(&vm->lock);
    long r = do_brk(addr);
    mutex_unlock(&vm->lock);
    return r;
}
