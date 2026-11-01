#pragma once

struct file;
struct fs;
struct vnode;
struct blkdev;

struct fs *devfs_create();

int devfs_open(struct vnode *vn, int flags, struct file **out);

struct blkdev *devfs_blkdev(struct vnode *vn);
