#include <stddef.h>
#include <stdint.h>

#include <blk.h>
#include <errno.h>
#include <kmalloc.h>
#include <mutex.h>
#include <printf.h>
#include <sched.h>
#include <spinlock.h>
#include <virtio.h>
#include <waitq.h>

#include "../mm/pmm.h"
#include "pmap.h"

#define VIRTIO_BLK_ID 0x1042
#define T_IN          0
#define T_OUT         1
#define S_OK          0
#define MAX_DRIVES    4
#define CHUNK_SECTORS 128
#define CAP_CAPACITY  0

struct req_hdr {
    uint32_t type, reserved;
    uint64_t sector;
};

struct vblk {
    struct blkdev     dev;
    struct virtio_dev vd;
    struct virtq      q;
    struct mutex      lock;
    struct req_hdr   *hdr;
    volatile uint8_t *status;
    volatile int      done;
    struct waitq      wq;
    char              name[8];
};

static struct vblk drives[MAX_DRIVES];
static int         ndrives;

static void on_done(struct virtq *q) {
    struct vblk *v  = q->priv;
    uint64_t     fl = spin_lock_irqsave(&sched_lock);
    while (virtq_pop(q, nullptr)) {
        v->done = 1;
    }
    waitq_wakeup_all(&v->wq);
    spin_unlock_irqrestore(&sched_lock, fl);
}

static int add_data(struct vq_buf *b, int n, int max, void *buf, size_t len,
                    int write) {
    for (uint8_t *p = buf; len;) {
        size_t    in_page = PAGE_SIZE - ((uintptr_t)p & (PAGE_SIZE - 1));
        size_t    k       = len < in_page ? len : in_page;
        uintptr_t pa      = kvirt_to_phys(p);
        if (!pa || n == max) {
            return -1;
        }
        if (n > 1 && b[n - 1].pa + b[n - 1].len == pa &&
            b[n - 1].write == write) {
            b[n - 1].len += (uint32_t)k;
        } else {
            b[n++] = (struct vq_buf){pa, (uint32_t)k, write};
        }
        p += k;
        len -= k;
    }
    return n;
}

static int request(struct vblk *v, uint64_t lba, uint32_t count, void *buf,
                   int write) {
    struct vq_buf b[CHUNK_SECTORS * SECTOR_SIZE / PAGE_SIZE + 4];
    *v->hdr    = (struct req_hdr){.type = write ? T_OUT : T_IN, .sector = lba};
    *v->status = 0xff;
    b[0]       = (struct vq_buf){kvirt_to_phys(v->hdr), sizeof(*v->hdr), 0};
    int n      = add_data(b, 1, (int)(sizeof(b) / sizeof(*b)) - 1, buf,
                          (size_t)count * SECTOR_SIZE, !write);
    if (n < 0) {
        return -EIO;
    }
    b[n++]  = (struct vq_buf){kvirt_to_phys((void *)v->status), 1, 1};
    v->done = 0;
    int err = virtq_add(&v->q, b, n, v);
    if (err) {
        return err;
    }
    virtq_kick(&v->q);
    if (v->q.polled) {
        while (!virtq_pop(&v->q, nullptr)) {
        }
    } else {
        uint64_t fl = spin_lock_irqsave(&sched_lock);
        while (!v->done) {
            waitq_sleep(&v->wq);
        }
        spin_unlock_irqrestore(&sched_lock, fl);
    }
    return *v->status == S_OK ? 0 : -EIO;
}

static int vblk_io(struct blkdev *dev, uint64_t lba, uint32_t count, void *buf,
                   int write) {
    struct vblk *v = dev->priv;
    if (lba + count > dev->sectors) {
        return -EINVAL;
    }
    mutex_lock(&v->lock);
    int err = 0;
    for (uint8_t *p = buf; count && !err;) {
        uint32_t n = count > CHUNK_SECTORS ? CHUNK_SECTORS : count;
        err        = request(v, lba, n, p, write);
        lba += n;
        count -= n;
        p += (size_t)n * SECTOR_SIZE;
    }
    mutex_unlock(&v->lock);
    return err;
}

static int vblk_read(struct blkdev *dev, uint64_t lba, uint32_t count,
                     void *buf) {
    return vblk_io(dev, lba, count, buf, 0);
}

static int vblk_write(struct blkdev *dev, uint64_t lba, uint32_t count,
                      const void *buf) {
    return vblk_io(dev, lba, count, (void *)buf, 1);
}

static void attach(struct pci_dev *pci) {
    struct vblk *v = &drives[ndrives];
    uint64_t     features;
    if (virtio_open(pci, &v->vd) || !v->vd.config ||
        virtio_features(&v->vd, 0, &features)) {
        printf("virtio-blk: %02x:%02x.%x: not a device to drive\n", pci->bus,
               pci->slot, pci->func);
        return;
    }
    uintptr_t pa = pmm_alloc_page();
    if (!pa || virtq_setup(&v->vd, &v->q, 0, on_done, v)) {
        virtio_fail(&v->vd);
        return;
    }
    v->hdr    = phys_to_virt(pa);
    v->status = (volatile uint8_t *)(v->hdr + 1);
    mutex_init(&v->lock);
    waitq_init(&v->wq);
    uint64_t sectors =
        *(volatile uint32_t *)(v->vd.config + CAP_CAPACITY) |
        (uint64_t)*(volatile uint32_t *)(v->vd.config + CAP_CAPACITY + 4) << 32;
    v->name[0] = 'v', v->name[1] = 'd', v->name[2] = (char)('a' + ndrives),
    v->name[3] = '\0';
    v->dev     = (struct blkdev){.name    = v->name,
                                 .sectors = sectors,
                                 .read    = vblk_read,
                                 .write   = vblk_write,
                                 .priv    = v};
    virtio_ready(&v->vd);
    printf("virtio-blk: %s, %lu sectors (%lu MiB)%s\n", v->name,
           (unsigned long)sectors, (unsigned long)(sectors >> 11),
           v->q.polled ? ", polled" : "");
    blk_register(&v->dev);
    ndrives++;
}

void virtio_blk_init() {
    for (int i = 0; pci_at(i) && ndrives < MAX_DRIVES; i++) {
        struct pci_dev *d = pci_at(i);
        if (d->vendor == VIRTIO_VENDOR && d->device == VIRTIO_BLK_ID) {
            attach(d);
        }
    }
}
