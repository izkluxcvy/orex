#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <kmalloc.h>
#include <memcpy.h>
#include <memset.h>
#include <mutex.h>
#include <spinlock.h>
#include <vfs.h>
#include <vm.h>
#include <vobj.h>

#include "../mm/pmm.h"
#include "pmap.h"

#define FAN   512
#define LEVEL 9
#define LIMIT ((uint64_t)FAN * FAN * FAN)

static struct spinlock obj_lock = {.name = "vobj"};
static uint64_t        ndirty;

static void set_dirty(uintptr_t *s) {
    if (!(*s & VOBJ_DIRTY)) {
        *s |= VOBJ_DIRTY;
        __atomic_add_fetch(&ndirty, 1, __ATOMIC_RELAXED);
    }
}

static void clear_dirty(uintptr_t *s) {
    if (*s & VOBJ_DIRTY) {
        *s &= ~(uintptr_t)VOBJ_DIRTY;
        __atomic_sub_fetch(&ndirty, 1, __ATOMIC_RELAXED);
    }
}

struct vobj *vobj_new(struct vnode *backing) {
    struct vobj *o = kmalloc(sizeof(*o));
    if (o) {
        *o = (struct vobj){.refs = 1, .backing = backing};
        mutex_init(&o->lock);
    }
    return o;
}

struct vobj *vobj_ref(struct vobj *o) {
    uint64_t f = spin_lock_irqsave(&obj_lock);
    o->refs++;
    spin_unlock_irqrestore(&obj_lock, f);
    return o;
}

void vobj_link(struct vobj *o, struct vm_area *a) {
    uint64_t f  = spin_lock_irqsave(&obj_lock);
    a->obj_next = o->areas;
    o->areas    = a;
    spin_unlock_irqrestore(&obj_lock, f);
}

void vobj_unlink(struct vobj *o, struct vm_area *a) {
    uint64_t f = spin_lock_irqsave(&obj_lock);
    for (struct vm_area **pp = &o->areas; *pp; pp = &(*pp)->obj_next) {
        if (*pp == a) {
            *pp = a->obj_next;
            break;
        }
    }
    spin_unlock_irqrestore(&obj_lock, f);
}

static void unmap_from(struct vobj *o, uint64_t keep) {
    uint64_t f = spin_lock_irqsave(&obj_lock);
    for (struct vm_area *a = o->areas; a; a = a->obj_next) {
        uint64_t first = a->off / PAGE_SIZE;
        for (uintptr_t va = a->start; va < a->end; va += PAGE_SIZE) {
            if (first + (va - a->start) / PAGE_SIZE >= keep) {
                pmap_remove(a->vm->pmap, va);
            }
        }
    }
    spin_unlock_irqrestore(&obj_lock, f);
}

static uintptr_t *slot(struct vobj *o, uint64_t idx, int make) {
    if (idx >= LIMIT) {
        return nullptr;
    }
    void **t = (void **)&o->root;
    for (int level = 2; level > 0; level--) {
        if (!*t) {
            if (!make || !(*t = kmalloc(FAN * sizeof(void *)))) {
                return nullptr;
            }
            memset(*t, 0, FAN * sizeof(void *));
        }
        t = (void **)*t + ((idx >> (LEVEL * level)) & (FAN - 1));
    }
    if (!*t) {
        if (!make || !(*t = kmalloc(FAN * sizeof(uintptr_t)))) {
            return nullptr;
        }
        memset(*t, 0, FAN * sizeof(uintptr_t));
    }
    return (uintptr_t *)*t + (idx & (FAN - 1));
}

static void each(struct vobj *o, uint64_t first,
                 void (*fn)(struct vobj *, uint64_t, uintptr_t *, void *),
                 void *ctx) {
    uintptr_t **l1 = o->root;
    for (uint64_t i = 0; l1 && i < FAN; i++) {
        uintptr_t **l2 = (uintptr_t **)l1[i];
        for (uint64_t j = 0; l2 && j < FAN; j++) {
            uintptr_t *l3 = l2[j];
            for (uint64_t k = 0; l3 && k < FAN; k++) {
                uint64_t idx = (i << (2 * LEVEL)) | (j << LEVEL) | k;
                if (l3[k] && idx >= first) {
                    fn(o, idx, &l3[k], ctx);
                }
            }
        }
    }
}

static void free_tables(uintptr_t **l1) {
    for (int i = 0; l1 && i < FAN; i++) {
        uintptr_t **l2 = (uintptr_t **)l1[i];
        for (int j = 0; l2 && j < FAN; j++) {
            kfree(l2[j]);
        }
        kfree(l2);
    }
    kfree(l1);
}

static void drop_page(struct vobj *o, uint64_t idx, uintptr_t *s, void *ctx) {
    (void)idx, (void)ctx;
    clear_dirty(s);
    pmm_page_unref(*s & PMAP_ADDR);
    *s = 0;
    o->npages--;
}

void vobj_put(struct vobj *o) {
    uint64_t f    = spin_lock_irqsave(&obj_lock);
    int      last = --o->refs == 0;
    spin_unlock_irqrestore(&obj_lock, f);
    if (last) {
        if (o->backing && o->backing->nlink) {
            vobj_sync(o);
        }
        each(o, 0, drop_page, nullptr);
        free_tables(o->root);
        kfree(o);
    }
}

uintptr_t vobj_lookup(struct vobj *o, uint64_t idx) {
    mutex_lock(&o->lock);
    uintptr_t *s  = slot(o, idx, 0);
    uintptr_t  pa = s ? *s & PMAP_ADDR : 0;
    mutex_unlock(&o->lock);
    return pa;
}

int vobj_page(struct vobj *o, uint64_t idx, int dirty, uintptr_t *pa) {
    mutex_lock(&o->lock);
    uintptr_t *s = slot(o, idx, 1);
    if (!s) {
        mutex_unlock(&o->lock);
        return -ENOMEM;
    }
    if (!*s) {
        mutex_unlock(&o->lock);
        uintptr_t fresh = pmm_alloc_page();
        if (!fresh) {
            return -ENOMEM;
        }
        struct vnode *vn = o->backing;
        if (vn && idx * PAGE_SIZE < vn->size) {
            uint64_t left = vn->size - idx * PAGE_SIZE;
            long     r    = vnode_read(vn, phys_to_virt(fresh),
                                       left < PAGE_SIZE ? left : PAGE_SIZE,
                                       idx * PAGE_SIZE);
            if (r < 0) {
                pmm_free_page(fresh);
                return (int)r;
            }
        }
        mutex_lock(&o->lock);
        s = slot(o, idx, 1);
        if (!*s) {
            *s = fresh;
            o->npages++;
        } else {
            pmm_free_page(fresh);
        }
    }
    if (dirty & VOBJ_DIRTY) {
        set_dirty(s);
    }
    *pa = *s & PMAP_ADDR;
    if (dirty & VOBJ_HOLD) {
        pmm_page_ref(*pa);
    }
    mutex_unlock(&o->lock);
    return 0;
}

static int span(struct vobj *o, uint64_t off, size_t n, int make,
                void (*fn)(uint8_t *page, size_t in, size_t len, char *buf),
                char *buf) {
    mutex_lock(&o->lock);
    for (size_t done = 0; done < n;) {
        uint64_t   idx = (off + done) / PAGE_SIZE;
        size_t     in  = (off + done) % PAGE_SIZE;
        size_t     len = PAGE_SIZE - in < n - done ? PAGE_SIZE - in : n - done;
        uintptr_t *s   = slot(o, idx, make);
        if (make && s && !*s) {
            *s = pmm_alloc_page();
        }
        if (make && (!s || !*s)) {
            mutex_unlock(&o->lock);
            return -ENOSPC;
        }
        if (make) {
            set_dirty(s);
        }
        fn(s && *s ? (uint8_t *)phys_to_virt(*s & PMAP_ADDR) : nullptr, in, len,
           buf + done);
        done += len;
    }
    mutex_unlock(&o->lock);
    return 0;
}

static void out_or_zero(uint8_t *page, size_t in, size_t len, char *buf) {
    if (page) {
        memcpy(buf, page + in, len);
    } else {
        memset(buf, 0, len);
    }
}

static void out_if_there(uint8_t *page, size_t in, size_t len, char *buf) {
    if (page) {
        memcpy(buf, page + in, len);
    }
}

static void in_if_there(uint8_t *page, size_t in, size_t len, char *buf) {
    if (page) {
        memcpy(page + in, buf, len);
    }
}

void vobj_read(struct vobj *o, uint64_t off, void *buf, size_t n) {
    span(o, off, n, 0, out_or_zero, buf);
}

int vobj_write(struct vobj *o, uint64_t off, const void *buf, size_t n) {
    return span(o, off, n, 1, in_if_there, (char *)buf);
}

void vobj_overlay(struct vobj *o, uint64_t off, void *buf, size_t n) {
    span(o, off, n, 0, out_if_there, buf);
}

void vobj_update(struct vobj *o, uint64_t off, const void *buf, size_t n) {
    span(o, off, n, 0, in_if_there, (char *)buf);
}

void vobj_truncate(struct vobj *o, uint64_t size) {
    mutex_lock(&o->lock);
    uint64_t keep = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    unmap_from(o, keep);
    each(o, keep, drop_page, nullptr);
    uintptr_t *s = size % PAGE_SIZE ? slot(o, size / PAGE_SIZE, 0) : nullptr;
    if (s && *s) {
        size_t in = size % PAGE_SIZE;
        memset((uint8_t *)phys_to_virt(*s & PMAP_ADDR) + in, 0, PAGE_SIZE - in);
    }
    mutex_unlock(&o->lock);
}

struct dirty_list {
    uint64_t  *idx;
    uintptr_t *pa;
    size_t     n, cap;
};

static void note_dirty(struct vobj *o, uint64_t idx, uintptr_t *s, void *ctx) {
    (void)o;
    struct dirty_list *d = ctx;
    if ((*s & VOBJ_DIRTY) && d->n < d->cap) {
        d->idx[d->n]  = idx;
        d->pa[d->n++] = *s & PMAP_ADDR;
        pmm_page_ref(*s & PMAP_ADDR);
    }
}

static void clean(struct vobj *o, uint64_t idx, uintptr_t pa) {
    uintptr_t *s = slot(o, idx, 0);
    if (s && (*s & PMAP_ADDR) == pa) {
        clear_dirty(s);
    }
}

int vobj_sync(struct vobj *o) {
    struct vnode *vn = o->backing;
    if (!vn || !vn->fs->vops->write || !vn->nlink) {
        return 0;
    }
    struct dirty_list d = {.cap = 4096};
    d.idx               = kmalloc(d.cap * sizeof(*d.idx));
    d.pa                = kmalloc(d.cap * sizeof(*d.pa));
    if (!d.idx || !d.pa) {
        kfree(d.idx);
        kfree(d.pa);
        return -ENOMEM;
    }
    int err = 0;
    for (uint64_t from = 0;;) {
        d.n = 0;
        mutex_lock(&o->lock);
        each(o, from, note_dirty, &d);
        mutex_unlock(&o->lock);
        for (size_t i = 0; i < d.n;) {
            size_t   run  = 1;
            uint64_t off  = d.idx[i] * PAGE_SIZE;
            uint64_t left = off < vn->size ? vn->size - off : 0;
            long     r    = left ? vn->fs->vops->write(
                                       vn, phys_to_virt(d.pa[i]),
                                       left < PAGE_SIZE ? left : PAGE_SIZE, off)
                                 : 0;
            if (r < 0 && !err) {
                err = (int)r;
            }
            if (r >= 0 && !o->areas) {
                mutex_lock(&o->lock);
                for (size_t k = 0; k < run; k++) {
                    clean(o, d.idx[i + k], d.pa[i + k]);
                }
                mutex_unlock(&o->lock);
            }
            i += run;
        }
        for (size_t i = 0; i < d.n; i++) {
            pmm_page_unref(d.pa[i]);
        }
        if (d.n < d.cap) {
            break;
        }
        from = d.idx[d.n - 1] + 1;
    }
    kfree(d.idx);
    kfree(d.pa);
    return err;
}
