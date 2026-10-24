#pragma once

struct blkdev;
struct fs;

struct fs *ext2_mount(struct blkdev *dev, int ro);
int        ext2_clean(struct blkdev *dev);
int        ext2_uuid(struct blkdev *dev, char out[37]);
