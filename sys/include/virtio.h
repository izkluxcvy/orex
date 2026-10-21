#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pci.h>
#include <spinlock.h>

// Virtio 1.0 over PCI

#define VIRTIO_VENDOR      0x1af4
#define VIRTIO_F_VERSION_1 (1ULL << 32)

struct virtio_dev {
    struct pci_dev   *pci;
    volatile uint8_t *common, *isr, *config;
    volatile uint8_t *notify;
    uint32_t          notify_mul;
};

struct vq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags, next;
};

struct vq_avail {
    uint16_t flags, idx, ring[];
};

struct vq_used_elem {
    uint32_t id, len;
};

struct vq_used {
    uint16_t            flags, idx;
    struct vq_used_elem ring[];
};

struct virtq {
    struct virtio_dev *vd;
    uint16_t           index, size;
    struct vq_desc    *desc;
    struct vq_avail   *avail;
    struct vq_used    *used;
    volatile uint16_t *notify;
    uint16_t           free_head, num_free, last_used;
    void             **cookie;
    uint16_t          *chain_len;
    struct spinlock    lock;
    int                polled;
    void (*done)(struct virtq *q);
    void *priv;
};

struct vq_buf {
    uintptr_t pa;
    uint32_t  len;
    int       write;
};

int  virtio_open(struct pci_dev *pci, struct virtio_dev *vd);
int  virtio_features(struct virtio_dev *vd, uint64_t wanted, uint64_t *got);
int  virtq_setup(struct virtio_dev *vd, struct virtq *q, uint16_t index,
                 void (*done)(struct virtq *q), void *priv);
void virtio_ready(struct virtio_dev *vd);
void virtio_fail(struct virtio_dev *vd);

int   virtq_add(struct virtq *q, const struct vq_buf *b, int n, void *cookie);
void  virtq_kick(struct virtq *q);
void *virtq_pop(struct virtq *q, uint32_t *len);

void virtio_blk_init();
