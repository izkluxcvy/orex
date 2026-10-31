#pragma once

#include <stddef.h>
#include <stdint.h>

#include <mutex.h>

struct vnode;
struct vm_area;

struct vobj {
    int             refs;
    struct vm_area *areas;
    struct mutex    lock;
    uintptr_t     **root;
    struct vnode   *backing;
    uint64_t        npages;
};

#define VOBJ_DIRTY  1
#define VOBJ_HOLD   2
#define VOBJ_NOFILL 4

struct vobj *vobj_new(struct vnode *backing);
struct vobj *vobj_ref(struct vobj *o);
void         vobj_put(struct vobj *o);
void         vobj_link(struct vobj *o, struct vm_area *a);
void         vobj_unlink(struct vobj *o, struct vm_area *a);

int       vobj_page(struct vobj *o, uint64_t idx, int how, uintptr_t *pa);
uintptr_t vobj_lookup(struct vobj *o, uint64_t idx);

void vobj_read(struct vobj *o, uint64_t off, void *buf, size_t n);
int  vobj_write(struct vobj *o, uint64_t off, const void *buf, size_t n);
void vobj_overlay(struct vobj *o, uint64_t off, void *buf, size_t n);
void vobj_update(struct vobj *o, uint64_t off, const void *buf, size_t n);

void vobj_truncate(struct vobj *o, uint64_t size);
int  vobj_sync(struct vobj *o);
