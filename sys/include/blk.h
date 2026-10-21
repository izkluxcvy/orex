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
    void *priv;
};

void           blk_register(struct blkdev *dev);
struct blkdev *blk_find(const char *name);
struct blkdev *blk_at(int index);
