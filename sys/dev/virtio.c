#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <kmalloc.h>
#include <memset.h>
#include <printf.h>
#include <virtio.h>

#include "../mm/pmm.h"
#include "irq.h"
#include "pmap.h"

#define CAP_VENDOR     0x09
#define CAP_CFG_TYPE   3
#define CAP_BAR        4
#define CAP_OFFSET     8
#define CAP_LENGTH     12
#define CAP_NOTIFY_MUL 16
#define CFG_COMMON     1
#define CFG_NOTIFY     2
#define CFG_ISR        3
#define CFG_DEVICE     4

#define C_DFSELECT 0x00
#define C_DF       0x04
#define C_GFSELECT 0x08
#define C_GF       0x0c
#define C_MSIX     0x10
#define C_NUMQ     0x12
#define C_STATUS   0x14
#define C_Q_SELECT 0x16
#define C_Q_SIZE   0x18
#define C_Q_MSIX   0x1a
#define C_Q_ENABLE 0x1c
#define C_Q_NOFF   0x1e
#define C_Q_DESC   0x20
#define C_Q_DRIVER 0x28
#define C_Q_DEVICE 0x30

#define S_ACKNOWLEDGE 1
#define S_DRIVER      2
#define S_DRIVER_OK   4
#define S_FEATURES_OK 8
#define S_FAILED      128

#define DESC_NEXT  1
#define DESC_WRITE 2
#define NO_VECTOR  0xffff
#define MAX_QSIZE  256

#define R8(p, o)  (*(volatile uint8_t *)((p) + (o)))
#define R16(p, o) (*(volatile uint16_t *)((p) + (o)))
#define R32(p, o) (*(volatile uint32_t *)((p) + (o)))

static struct virtq *by_vector[256];

static void write64(volatile uint8_t *p, unsigned off, uint64_t v) {
    R32(p, off)     = (uint32_t)v;
    R32(p, off + 4) = (uint32_t)(v >> 32);
}

static volatile uint8_t *map_cap(struct pci_dev *pci, uint8_t cap,
                                 size_t *len) {
    uint8_t  bar = pci_read8(pci, cap + CAP_BAR);
    uint32_t off = pci_read32(pci, cap + CAP_OFFSET);
    uint32_t n   = pci_read32(pci, cap + CAP_LENGTH);
    if (len) {
        *len = n;
    }
    return n ? pci_map_bar(pci, bar, off, n) : nullptr;
}

int virtio_open(struct pci_dev *pci, struct virtio_dev *vd) {
    *vd = (struct virtio_dev){.pci = pci};
    for (uint8_t cap = pci_find_cap(pci, CAP_VENDOR, 0); cap;
         cap         = pci_find_cap(pci, CAP_VENDOR, cap)) {
        switch (pci_read8(pci, cap + CAP_CFG_TYPE)) {
        case CFG_COMMON:
            vd->common = vd->common ? vd->common : map_cap(pci, cap, nullptr);
            break;
        case CFG_NOTIFY:
            if (!vd->notify) {
                vd->notify     = map_cap(pci, cap, nullptr);
                vd->notify_mul = pci_read32(pci, cap + CAP_NOTIFY_MUL);
            }
            break;
        case CFG_ISR:
            vd->isr = vd->isr ? vd->isr : map_cap(pci, cap, nullptr);
            break;
        case CFG_DEVICE:
            vd->config = vd->config ? vd->config : map_cap(pci, cap, nullptr);
            break;
        }
    }
    if (!vd->common || !vd->notify) {
        return -ENODEV;
    }
    pci_enable(pci);
    R8(vd->common, C_STATUS) = 0;
    for (int i = 0; i < 1000000 && R8(vd->common, C_STATUS); i++) {
    }
    R8(vd->common, C_STATUS) = S_ACKNOWLEDGE;
    R8(vd->common, C_STATUS) = S_ACKNOWLEDGE | S_DRIVER;
    return 0;
}

int virtio_features(struct virtio_dev *vd, uint64_t wanted, uint64_t *got) {
    R32(vd->common, C_DFSELECT) = 0;
    uint64_t have               = R32(vd->common, C_DF);
    R32(vd->common, C_DFSELECT) = 1;
    have |= (uint64_t)R32(vd->common, C_DF) << 32;
    uint64_t use = have & (wanted | VIRTIO_F_VERSION_1);
    if (!(use & VIRTIO_F_VERSION_1)) {
        return -ENODEV;
    }
    R32(vd->common, C_GFSELECT) = 0;
    R32(vd->common, C_GF)       = (uint32_t)use;
    R32(vd->common, C_GFSELECT) = 1;
    R32(vd->common, C_GF)       = (uint32_t)(use >> 32);
    R8(vd->common, C_STATUS) |= S_FEATURES_OK;
    if (!(R8(vd->common, C_STATUS) & S_FEATURES_OK)) {
        return -ENODEV;
    }
    *got = use;
    return 0;
}

static void on_interrupt(struct trapframe *tf) {
    struct virtq *q = by_vector[tf->vector & 0xff];
    if (q && q->done) {
        q->done(q);
    }
}

static void *page() {
    uintptr_t pa = pmm_alloc_page();
    if (!pa) {
        return nullptr;
    }
    memset(phys_to_virt(pa), 0, PAGE_SIZE);
    return phys_to_virt(pa);
}

int virtq_setup(struct virtio_dev *vd, struct virtq *q, uint16_t index,
                void (*done)(struct virtq *q), void *priv) {
    R16(vd->common, C_Q_SELECT) = index;
    uint16_t size               = R16(vd->common, C_Q_SIZE);
    if (!size) {
        return -ENODEV;
    }
    size = size > MAX_QSIZE ? MAX_QSIZE : size;
    *q   = (struct virtq){
        .vd = vd, .index = index, .size = size, .done = done, .priv = priv};
    spin_lock_init(&q->lock, "virtq");
    q->desc      = page();
    q->avail     = page();
    q->used      = page();
    q->cookie    = kmalloc(size * sizeof(*q->cookie));
    q->chain_len = kmalloc(size * sizeof(*q->chain_len));
    if (!q->desc || !q->avail || !q->used || !q->cookie || !q->chain_len) {
        return -ENOMEM;
    }
    for (uint16_t i = 0; i < size; i++) {
        q->desc[i].next = (uint16_t)(i + 1);
    }
    q->num_free = size;
    q->notify   = (volatile uint16_t *)(vd->notify + R16(vd->common, C_Q_NOFF) *
                                                         vd->notify_mul);

    uint8_t vector = irq_alloc_vector();
    q->polled      = 1;
    if (vector && pci_msix_route(vd->pci, index, vector) == 0) {
        by_vector[vector] = q;
        irq_register(vector, on_interrupt);
        R16(vd->common, C_Q_MSIX) = index;
        q->polled                 = R16(vd->common, C_Q_MSIX) != index;
    }
    if (q->polled) {
        R16(vd->common, C_Q_MSIX) = NO_VECTOR;
    }
    R16(vd->common, C_MSIX)   = NO_VECTOR;
    R16(vd->common, C_Q_SIZE) = size;
    write64(vd->common, C_Q_DESC, kvirt_to_phys(q->desc));
    write64(vd->common, C_Q_DRIVER, kvirt_to_phys(q->avail));
    write64(vd->common, C_Q_DEVICE, kvirt_to_phys(q->used));
    R16(vd->common, C_Q_ENABLE) = 1;
    return 0;
}

void virtio_ready(struct virtio_dev *vd) {
    R8(vd->common, C_STATUS) |= S_DRIVER_OK;
}
void virtio_fail(struct virtio_dev *vd) {
    R8(vd->common, C_STATUS) |= S_FAILED;
}

int virtq_add(struct virtq *q, const struct vq_buf *b, int n, void *cookie) {
    uint64_t fl = spin_lock_irqsave(&q->lock);
    if (n <= 0 || n > q->num_free) {
        spin_unlock_irqrestore(&q->lock, fl);
        return -ENOSPC;
    }
    uint16_t head = q->free_head, at = head;
    for (int i = 0; i < n; i++) {
        struct vq_desc *d = &q->desc[at];
        d->addr           = b[i].pa;
        d->len            = b[i].len;
        d->flags          = (uint16_t)((b[i].write ? DESC_WRITE : 0) |
                                       (i + 1 < n ? DESC_NEXT : 0));
        at                = i + 1 < n ? d->next : at;
    }
    q->free_head = q->desc[at].next;
    q->num_free -= (uint16_t)n;
    q->cookie[head]                         = cookie;
    q->chain_len[head]                      = (uint16_t)n;
    q->avail->ring[q->avail->idx % q->size] = head;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    q->avail->idx++;
    spin_unlock_irqrestore(&q->lock, fl);
    return 0;
}

void virtq_kick(struct virtq *q) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    *q->notify = q->index;
}

void *virtq_pop(struct virtq *q, uint32_t *len) {
    uint64_t fl = spin_lock_irqsave(&q->lock);
    if (q->last_used == __atomic_load_n(&q->used->idx, __ATOMIC_ACQUIRE)) {
        spin_unlock_irqrestore(&q->lock, fl);
        return nullptr;
    }
    struct vq_used_elem e = q->used->ring[q->last_used % q->size];
    q->last_used++;
    uint16_t head = (uint16_t)e.id, last = head;
    for (uint16_t i = 1; i < q->chain_len[head]; i++) {
        last = q->desc[last].next;
    }
    q->desc[last].next = q->free_head;
    q->free_head       = head;
    q->num_free += q->chain_len[head];
    void *cookie = q->cookie[head];
    spin_unlock_irqrestore(&q->lock, fl);
    if (len) {
        *len = e.len;
    }
    return cookie;
}
