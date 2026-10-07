#pragma once

#include <stddef.h>
#include <stdint.h>

#include <mutex.h>

struct pmap;
struct vnode;

#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20

#define USER_STACK_MAX (8UL << 20)
#define MMAP_GAP       (1UL << 20)

struct vm_area {
    uintptr_t       start, end;
    int             prot;
    struct vnode   *vn;
    uint64_t        off;
    uintptr_t       file_end;
    struct vm_area *next;
};

struct vmspace {
    struct mutex    lock;
    struct pmap    *pmap;
    struct vm_area *areas;
    uintptr_t       brk_base, brk;
    uintptr_t       stack_top, mmap_top;
};

struct vmspace *vm_create();
void            vm_destroy(struct vmspace *vm);
void            vm_clear(struct vmspace *vm);

int vm_map(struct vmspace *vm, uintptr_t start, uintptr_t end, int prot,
           struct vnode *vn, uint64_t off, uintptr_t file_end);
int vm_unmap(struct vmspace *vm, uintptr_t start, uintptr_t end);

int vm_fault(struct vmspace *vm, uintptr_t va, int access);

int vm_write(struct vmspace *vm, uintptr_t va, const void *src, size_t len);

long sys_mmap(uintptr_t addr, size_t len, long prot, long flags, long fd,
              long off);
long sys_munmap(uintptr_t addr, size_t len);
long sys_mprotect(uintptr_t addr, size_t len, long prot);
long sys_brk(uintptr_t addr);
