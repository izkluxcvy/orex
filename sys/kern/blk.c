#include <stddef.h>
#include <stdint.h>

#include <blk.h>
#include <errno.h>
#include <kmalloc.h>
#include <ktime.h>
#include <memcmp.h>
#include <memcpy.h>
#include <memset.h>
#include <mutex.h>
#include <printf.h>
#include <sched.h>
#include <spinlock.h>
#include <thread.h>

#include "../mm/pmm.h"
#include "pmap.h"

#define MAX_DEVS 32

static struct blkdev *devs[MAX_DEVS];
static int            ndevs;

void blk_register(struct blkdev *dev) {
    if (ndevs < MAX_DEVS) {
        devs[ndevs++] = dev;
    }
}

struct blkdev *blk_at(int index) {
    return index >= 0 && index < ndevs ? devs[index] : nullptr;
}

struct blkdev *blk_find(const char *name) {
    for (int i = 0; i < ndevs; i++) {
        const char *a = devs[i]->name, *b = name;
        while (*a && *a == *b) {
            a++, b++;
        }
        if (*a == *b) {
            return devs[i];
        }
    }
    return nullptr;
}

struct part {
    struct blkdev  dev;
    struct blkdev *whole;
    uint64_t       start;
    char           name[16];
};

static int part_read(struct blkdev *d, uint64_t lba, uint32_t n, void *buf) {
    struct part *p = d->priv;
    return lba + n > d->sectors
               ? -EIO
               : p->whole->read(p->whole, p->start + lba, n, buf);
}

static int part_write(struct blkdev *d, uint64_t lba, uint32_t n,
                      const void *buf) {
    struct part *p = d->priv;
    return lba + n > d->sectors
               ? -EIO
               : p->whole->write(p->whole, p->start + lba, n, buf);
}

#define GPT_SIG 0x5452415020494645ULL // "EFI PART"

static void scan_gpt(struct blkdev *disk) {
    uint8_t *sec = kmalloc(512);
    if (!sec || disk->read(disk, 1, 1, sec) != 0) {
        kfree(sec);
        return;
    }
    uint64_t sig, table;
    uint32_t count, size;
    memcpy(&sig, sec, 8);
    memcpy(&table, sec + 72, 8);
    memcpy(&count, sec + 80, 4);
    memcpy(&size, sec + 84, 4);
    if (sig != GPT_SIG || size < 128 || size > 512 || count > 256) {
        kfree(sec);
        return;
    }
    int found = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint64_t off = (uint64_t)i * size;
        if (off % 512 == 0 &&
            disk->read(disk, table + off / 512, 1, sec) != 0) {
            break;
        }
        const uint8_t *e = sec + off % 512;
        uint64_t       first, last;
        memcpy(&first, e + 32, 8);
        memcpy(&last, e + 40, 8);
        int used = 0;
        for (int k = 0; k < 16; k++) {
            used |= e[k];
        }
        if (!used || last < first || last >= disk->sectors ||
            ndevs == MAX_DEVS) {
            continue;
        }
        struct part *p = kmalloc(sizeof(*p));
        if (!p) {
            break;
        }
        *p         = (struct part){.whole = disk, .start = first};
        unsigned n = 0;
        for (const char *c = disk->name; *c && n < sizeof(p->name) - 4; c++) {
            p->name[n++] = *c;
        }
        if (i + 1 >= 10) {
            p->name[n++] = (char)('0' + (i + 1) / 10);
        }
        p->name[n++] = (char)('0' + (i + 1) % 10);
        p->name[n]   = '\0';
        p->dev       = (struct blkdev){.name    = p->name,
                                       .sectors = last - first + 1,
                                       .read    = part_read,
                                       .write   = part_write,
                                       .priv    = p,
                                       .whole   = disk};
        blk_register(&p->dev);
        found++;
    }
    if (found) {
        printf("blk: %s: %d GPT partitions found\n", disk->name, found);
    }
    kfree(sec);
}

void blk_scan_partitions() {
    int n = ndevs;
    for (int i = 0; i < n; i++) {
        if (!devs[i]->whole) {
            scan_gpt(devs[i]);
        }
    }
}

#define HASH 4096

static struct mutex bcache_lock;
static struct buf  *hash[HASH];
static struct buf  *newest, *oldest;
static size_t       nbuf, cache_bytes;

size_t blk_cache_bytes() { return cache_bytes; }

static unsigned hash_of(struct blkdev *dev, uint64_t block) {
    return (unsigned)((block * 0x9e3779b97f4a7c15ULL ^ (uintptr_t)dev) >> 40) %
           HASH;
}

static void lru_unlink(struct buf *b) {
    *(b->newer ? &b->newer->older : &newest) = b->older;
    *(b->older ? &b->older->newer : &oldest) = b->newer;
}

static void lru_push(struct buf *b) {
    b->newer                             = nullptr;
    b->older                             = newest;
    *(newest ? &newest->newer : &oldest) = b;
    newest                               = b;
}

static void hash_unlink(struct buf *b) {
    for (struct buf **pp = &hash[hash_of(b->dev, b->block)]; *pp;
         pp              = &(*pp)->hnext) {
        if (*pp == b) {
            *pp = b->hnext;
            return;
        }
    }
}

static uint8_t *data_alloc(size_t size) {
    if (size == PAGE_SIZE) {
        uintptr_t pa = pmm_alloc_page();
        return pa ? phys_to_virt(pa) : nullptr;
    }
    return kmalloc(size);
}

static void data_free(uint8_t *p, size_t size) {
    size == PAGE_SIZE ? pmm_free_page((uintptr_t)p - DMAP_BASE) : kfree(p);
}

static int may_grow() {
    size_t total = pmm_total_pages();
    return nbuf < total / 4 && pmm_free_pages() > total / 8;
}

static int write_out(struct buf *b) {
    uint32_t per = (uint32_t)(b->size / SECTOR_SIZE);
    int      err = b->dev->write(b->dev, b->block * per, per, b->data);
    if (!err) {
        b->dirty = 0;
    }
    return err;
}

static struct buf *take_victim() {
    for (struct buf *b = oldest; b; b = b->newer) {
        if (b->refs == 0) {
            if (b->dirty && write_out(b) != 0) {
                printf("blk: %s: block %lu lost, cannot write it\n",
                       b->dev->name, (unsigned long)b->block);
            }
            lru_unlink(b);
            hash_unlink(b);
            nbuf--;
            cache_bytes -= b->size;
            return b;
        }
    }
    return nullptr;
}

static int get(struct blkdev *dev, uint64_t block, size_t size, int fill,
               struct buf **out) {
    if (size == 0 || size % SECTOR_SIZE) {
        return -EINVAL;
    }

    mutex_lock(&bcache_lock);
    unsigned h = hash_of(dev, block);
    for (struct buf *b = hash[h]; b; b = b->hnext) {
        if (b->dev == dev && b->block == block && b->size == size) {
            lru_unlink(b);
            lru_push(b);
            b->refs++;
            mutex_unlock(&bcache_lock);
            *out = b;
            return 0;
        }
    }

    // Cache miss
    struct buf *b = may_grow() ? nullptr : take_victim();
    if (b && b->size != size) {
        data_free(b->data, b->size);
        b->data = data_alloc(size);
        if (!b->data) {
            kfree(b);
            b = nullptr;
        }
    }
    if (!b && (b = kmalloc(sizeof(*b))) != nullptr &&
        !(b->data = data_alloc(size))) {
        kfree(b);
        b = nullptr;
    }
    if (!b) {
        mutex_unlock(&bcache_lock);
        return -ENOMEM;
    }

    uint32_t per = (uint32_t)(size / SECTOR_SIZE);
    int      err = 0;
    if (fill) {
        err = dev->read(dev, block * per, per, b->data);
    } else {
        memset(b->data, 0, size);
    }
    if (err) {
        data_free(b->data, size);
        kfree(b);
        mutex_unlock(&bcache_lock);
        return err;
    }

    uint8_t *data = b->data;
    *b            = (struct buf){.dev   = dev,
                                 .block = block,
                                 .size  = size,
                                 .data  = data,
                                 .refs  = 1,
                                 .hnext = hash[h]};
    hash[h]       = b;
    lru_push(b);
    nbuf++;
    cache_bytes += size;
    mutex_unlock(&bcache_lock);
    *out = b;
    return 0;
}

int bread(struct blkdev *dev, uint64_t block, size_t size, struct buf **out) {
    return get(dev, block, size, 1, out);
}

int bget(struct blkdev *dev, uint64_t block, size_t size, struct buf **out) {
    return get(dev, block, size, 0, out);
}

void brelse(struct buf *b) {
    mutex_lock(&bcache_lock);
    b->refs--;
    mutex_unlock(&bcache_lock);
}

int bwrite(struct buf *b) {
    b->dirty = 1;
    return 0;
}

int blk_sync() {
    int err = 0;
    mutex_lock(&bcache_lock);
    for (struct buf *b = oldest; b; b = b->newer) {
        if (b->dirty) {
            int e = write_out(b);
            err   = err ? err : e;
        }
    }
    mutex_unlock(&bcache_lock);
    return err;
}

static int kin(const struct blkdev *a, const struct blkdev *b) {
    return a == b || a->whole == b || b->whole == a ||
           (a->whole && a->whole == b->whole);
}

void blk_invalidate(struct blkdev *dev) {
    mutex_lock(&bcache_lock);
    for (struct buf *b = oldest, *next; b; b = next) {
        next = b->newer;
        if (b->refs == 0 && kin(b->dev, dev)) {
            if (b->dirty) {
                write_out(b);
            }
            lru_unlink(b);
            hash_unlink(b);
            nbuf--;
            cache_bytes -= b->size;
            data_free(b->data, b->size);
            kfree(b);
        }
    }
    mutex_unlock(&bcache_lock);
}

#define FLUSH_EVERY (5 * NSEC)

static void *flusher(void *arg) {
    (void)arg;
    while (1) {
        uint64_t fl = spin_lock_irqsave(&sched_lock);
        sleep_until(nullptr, time_mono() + FLUSH_EVERY);
        spin_unlock_irqrestore(&sched_lock, fl);
        blk_sync();
    }
    return nullptr;
}

void blk_start_flusher() {
    if (!thread_create("flusher", flusher, nullptr, SCHED_OTHER, 0)) {
        printf("blk: cannot start flusher thread");
    }
}

void bforget(struct blkdev *dev, uint64_t block, size_t size) {
    mutex_lock(&bcache_lock);
    for (struct buf *b = hash[hash_of(dev, block)]; b; b = b->hnext) {
        if (b->dev == dev && b->block == block && b->size == size) {
            b->dirty = 0;
            if (!b->refs) {
                lru_unlink(b);
                hash_unlink(b);
                nbuf--;
                cache_bytes -= b->size;
                data_free(b->data, b->size);
                kfree(b);
            }
            break;
        }
    }
    mutex_unlock(&bcache_lock);
}
