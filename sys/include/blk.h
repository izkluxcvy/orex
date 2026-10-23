#pragma once

#include <stddef.h>
#include <stdint.h>

#define SECTOR_SIZE 512

struct blkdev {
    const char *name;
    uint64_t    sectors;
    int (*read)(struct blkdev *dev, uint64_t lba, uint32_t count, void *buf);
    int (*write)(struct blkdev *dev, uint64_t lba, uint32_t count,
                 const void *buf);
    void          *priv;
    struct blkdev *whole;
};

void           blk_register(struct blkdev *dev);
void           blk_scan_partitions();
void           blk_invalidate(struct blkdev *dev);
struct blkdev *blk_find(const char *name);
struct blkdev *blk_at(int index);

struct buf {
    struct blkdev *dev;
    uint64_t       block;
    size_t         size;
    uint8_t       *data;
    int            refs;
    int            dirty;
    struct buf    *hnext;
    struct buf    *newer, *older;
};

int  bread(struct blkdev *dev, uint64_t block, size_t size, struct buf **out);
int  bget(struct blkdev *dev, uint64_t block, size_t size, struct buf **out);
void brelse(struct buf *b);

int    bwrite(struct buf *b);
int    blk_sync();
void   bforget(struct blkdev *dev, uint64_t block, size_t size);
void   blk_start_flusher();
size_t blk_cache_bytes();
