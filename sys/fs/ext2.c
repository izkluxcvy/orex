#include <stddef.h>
#include <stdint.h>

#include <blk.h>
#include <errno.h>
#include <ext2.h>
#include <kmalloc.h>
#include <memcmp.h>
#include <memcpy.h>
#include <memset.h>
#include <mutex.h>
#include <printf.h>
#include <vfs.h>

#define EXT2_MAGIC     0xef53
#define EXT2_ROOT_INO  2
#define SUPERBLOCK_OFF 1024
#define NDIRECT        12

#define INCOMPAT_FILETYPE    0x0002
#define RO_COMPAT_LARGE_FILE 0x0002

static uint8_t dt_of_ft(uint8_t ft) {
    static const uint8_t dt[8] = {DT_UNKNOWN, DT_REG,  DT_DIR,  DT_CHR,
                                  DT_BLK,     DT_FIFO, DT_SOCK, DT_LNK};
    return ft < 8 ? dt[ft] : DT_UNKNOWN;
}

struct ext2_super {
    uint32_t inodes_count, blocks_count, r_blocks_count;
    uint32_t free_blocks_count, free_inodes_count;
    uint32_t first_data_block, log_block_size, log_frag_size;
    uint32_t blocks_per_group, frags_per_group, inodes_per_group;
    uint32_t mtime, wtime;
    uint16_t mnt_count, max_mnt_count, magic, state, errors, minor_rev;
    uint32_t lastcheck, checkinterval, creator_os, rev_level;
    uint16_t def_resuid, def_resgid;
    uint32_t first_ino;
    uint16_t inode_size, block_group_nr;
    uint32_t feature_compat, feature_incompat, feature_ro_compat;
} __attribute__((packed));

struct ext2_group {
    uint32_t block_bitmap, inode_bitmap, inode_table;
    uint16_t free_blocks, free_inodes, used_dirs, pad;
    uint32_t reserved[3];
} __attribute__((packed));

struct ext2_inode {
    uint16_t mode, uid;
    uint32_t size, atime, ctime, mtime, dtime;
    uint16_t gid, links_count;
    uint32_t blocks, flags, osd1;
    uint32_t block[15];
    uint32_t generation, file_acl, size_high, faddr;
    uint16_t blocks_high, file_acl_high, uid_high, gid_high;
    uint16_t checksum_lo, reserved;
} __attribute__((packed));

struct ext2_dirent {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len, file_type;
    char     name[];
} __attribute__((packed));

#define STATE_VALID 1
#define STATE_ERROR 2

struct ext2_fs {
    struct fs      fs;
    struct blkdev *dev;
    struct mutex   lock;

    uint8_t            sb_raw[1024];
    struct ext2_super *sb;
    struct ext2_group *gdt;

    uint32_t block_size;
    uint32_t inode_size;
    uint32_t gdt_block;
    uint32_t groups;
};

static struct ext2_fs *efs_of(struct vnode *vn) {
    return (struct ext2_fs *)vn->fs;
}

static struct ext2_inode *inode_of(struct vnode *vn) { return vn->priv; }

// --- raw block access --------------------------------------------------------

static int read_bytes(struct ext2_fs *e, uint64_t block, uint32_t off,
                      void *dst, size_t len) {
    struct buf *b;
    int         err = bread(e->dev, block, e->block_size, &b);
    if (err) {
        return err;
    }
    memcpy(dst, b->data + off, len);
    brelse(b);
    return 0;
}

// --- inodes ------------------------------------------------------------------

static int inode_read(struct ext2_fs *e, uint32_t ino, struct ext2_inode *ip) {
    uint32_t group = (ino - 1) / e->sb->inodes_per_group;
    uint32_t index = (ino - 1) % e->sb->inodes_per_group;
    if (ino == 0 || group >= e->groups) {
        return -ENOENT;
    }

    uint64_t off   = (uint64_t)index * e->inode_size;
    uint64_t block = e->gdt[group].inode_table + off / e->block_size;
    uint32_t boff  = (uint32_t)(off % e->block_size);
    return read_bytes(e, block, boff, ip, sizeof(*ip));
}

static uint64_t file_size(struct ext2_fs *e, struct ext2_inode *ip) {
    uint64_t size = ip->size;
    if ((ip->mode & S_IFMT) == S_IFREG &&
        (e->sb->feature_ro_compat & RO_COMPAT_LARGE_FILE)) {
        size |= (uint64_t)ip->size_high << 32;
    }
    return size;
}

// --- block mapping -----------------------------------------------------------

static int bmap(struct ext2_fs *e, struct ext2_inode *ip, uint64_t fblock,
                int alloc, uint32_t *out) {
    uint32_t per = e->block_size / 4;
    int      err;
    (void)alloc;

    if (fblock < NDIRECT) {
        *out = ip->block[fblock];
        return 0;
    }
    fblock -= NDIRECT;

    int      level;
    uint64_t span = per;
    for (level = 1; level <= 3 && fblock >= span; level++) {
        fblock -= span;
        span *= per;
    }
    if (level > 3) {
        return -EFBIG;
    }

    uint32_t *slot  = &ip->block[NDIRECT + level - 1];
    uint32_t  block = *slot;

    for (; level > 0 && block; level--) {
        span /= per;
        uint32_t idx = (uint32_t)(fblock / span);
        fblock %= span;

        uint32_t next;
        if ((err = read_bytes(e, block, idx * 4, &next, 4)) != 0) {
            return err;
        }
        block = next;
    }
    *out = block;
    return 0;
}

static int has_blocks(struct vnode *vn) {
    uint32_t t = vn->mode & S_IFMT;
    return t == S_IFREG || t == S_IFDIR ||
           (t == S_IFLNK && inode_of(vn)->blocks != 0);
}

// --- file data ---------------------------------------------------------------

static long read_locked(struct vnode *vn, void *buf, size_t n, uint64_t off) {
    struct ext2_fs    *e   = efs_of(vn);
    struct ext2_inode *ip  = inode_of(vn);
    uint8_t           *out = buf;

    if (off >= vn->size) {
        return 0;
    }
    if (n > vn->size - off) {
        n = vn->size - off;
    }

    size_t done = 0;
    while (done < n) {
        uint64_t fblock = (off + done) / e->block_size;
        uint32_t boff   = (uint32_t)((off + done) % e->block_size);
        size_t   chunk  = e->block_size - boff;
        if (chunk > n - done) {
            chunk = n - done;
        }

        uint32_t block;
        int      err = bmap(e, ip, fblock, 0, &block);
        if (!err && block) {
            err = read_bytes(e, block, boff, out + done, chunk);
        } else if (!err) {
            memset(out + done, 0, chunk); // hole
        }
        if (err) {
            return done ? (long)done : err;
        }
        done += chunk;
    }
    return (long)done;
}

// --- directories -------------------------------------------------------------

static int next_dirent(struct vnode *dir, uint64_t *pos,
                       struct vfs_dirent *out) {
    struct ext2_fs *e = efs_of(dir);

    while (*pos < dir->size) {
        uint8_t             raw[sizeof(struct ext2_dirent) + NAME_MAX + 1];
        struct ext2_dirent *h    = (struct ext2_dirent *)raw;
        uint32_t            boff = (uint32_t)(*pos % e->block_size);
        size_t              room = e->block_size - boff;

        long got = read_locked(dir, raw,
                               room < sizeof(raw) ? room : sizeof(raw), *pos);
        if (got < (long)sizeof(*h)) {
            return got < 0 ? (int)got : -EIO;
        }
        if (h->rec_len < sizeof(*h) || h->rec_len > room ||
            (size_t)h->name_len + sizeof(*h) > h->rec_len) {
            return -EIO;
        }
        *pos += h->rec_len;
        if (!h->inode) {
            continue;
        }

        out->ino  = h->inode;
        out->type = dt_of_ft(h->file_type);
        memcpy(out->name, h->name, h->name_len);
        out->name[h->name_len] = '\0';
        return 1;
    }
    return 0;
}

static int lookup_locked(struct vnode *dir, const char *name, size_t len,
                         uint64_t *ino) {
    struct vfs_dirent *de = kmalloc(sizeof(*de));
    if (!de) {
        return -ENOMEM;
    }

    uint64_t pos = 0;
    int      got, err = -ENOENT;
    while ((got = next_dirent(dir, &pos, de)) > 0) {
        size_t n = 0;
        while (de->name[n]) {
            n++;
        }
        if (n == len && memcmp(de->name, name, len) == 0) {
            *ino = de->ino;
            err  = 0;
            break;
        }
    }
    if (got < 0) {
        err = got;
    }
    kfree(de);
    return err;
}

// --- vnode ops ---------------------------------------------------------------

static long ext2_read(struct vnode *vn, void *buf, size_t n, uint64_t off) {
    struct ext2_fs *e = efs_of(vn);
    mutex_lock(&e->lock);
    long got = read_locked(vn, buf, n, off);
    mutex_unlock(&e->lock);
    return got;
}

static long ext2_readlink(struct vnode *vn, char *buf, size_t n) {
    struct ext2_fs    *e  = efs_of(vn);
    struct ext2_inode *ip = inode_of(vn);
    if (n > vn->size) {
        n = vn->size;
    }
    if (!has_blocks(vn)) {
        memcpy(buf, ip->block, n);
        return (long)n;
    }
    mutex_lock(&e->lock);
    long got = read_locked(vn, buf, n, 0);
    mutex_unlock(&e->lock);
    return got;
}

static int ext2_readdir(struct vnode *dir, uint64_t *pos,
                        struct vfs_dirent *out) {
    struct ext2_fs *e = efs_of(dir);
    mutex_lock(&e->lock);
    int got = next_dirent(dir, pos, out);
    mutex_unlock(&e->lock);
    return got;
}

static int ext2_lookup(struct vnode *dir, const char *name, size_t len,
                       uint64_t *ino) {
    struct ext2_fs *e = efs_of(dir);
    mutex_lock(&e->lock);
    int err = lookup_locked(dir, name, len, ino);
    mutex_unlock(&e->lock);
    return err;
}

static uint64_t get_rdev(const struct ext2_inode *ip) {
    if (ip->block[0]) {
        return ip->block[0] & 0xffff;
    }
    uint32_t v = ip->block[1];
    return (uint64_t)((v >> 8) & 0xfff) << 8 |
           ((v & 0xff) | ((v >> 12) & ~0xffu));
}

static int ext2_vget(struct fs *fs, uint64_t ino, struct vnode *vn) {
    struct ext2_fs    *e  = (struct ext2_fs *)fs;
    struct ext2_inode *ip = kmalloc(sizeof(*ip));
    if (!ip) {
        return -ENOMEM;
    }
    mutex_lock(&e->lock);
    int err = inode_read(e, (uint32_t)ino, ip);
    mutex_unlock(&e->lock);
    if (err) {
        kfree(ip);
        return err;
    }

    vn->mode  = ip->mode;
    vn->uid   = ip->uid | (uint32_t)ip->uid_high << 16;
    vn->gid   = ip->gid | (uint32_t)ip->gid_high << 16;
    vn->nlink = ip->links_count;
    vn->size  = file_size(e, ip);
    vn->atime = ip->atime;
    vn->mtime = ip->mtime;
    vn->ctime = ip->ctime;
    vn->priv  = ip;
    if (S_ISCHR(vn->mode) || (vn->mode & S_IFMT) == S_IFBLK) {
        vn->rdev = get_rdev(ip);
    }
    return 0;
}

static void ext2_vput(struct vnode *vn) {
    struct ext2_inode *ip = inode_of(vn);
    kfree(ip);
}

static const struct vnode_ops ext2_vops_ro = {
    .lookup   = ext2_lookup,
    .read     = ext2_read,
    .readdir  = ext2_readdir,
    .readlink = ext2_readlink,
};

static const struct fs_ops ext2_ops = {.vget = ext2_vget, .vput = ext2_vput};

int ext2_uuid(struct blkdev *dev, char out[37]) {
    struct buf *b;
    if (bread(dev, SUPERBLOCK_OFF / 1024, 1024, &b) != 0) {
        return -1;
    }
    const struct ext2_super *sb    = (const struct ext2_super *)b->data;
    int                      ok    = sb->magic == EXT2_MAGIC;
    const uint8_t           *u     = b->data + 104; // s_uuid
    static const char        hex[] = "0123456789abcdef";
    char                    *o     = out;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            *o++ = '-';
        }
        *o++ = hex[u[i] >> 4];
        *o++ = hex[u[i] & 0xf];
    }
    *o = '\0';
    brelse(b);
    return ok ? 0 : -1;
}

int ext2_clean(struct blkdev *dev) {
    struct buf *b;
    if (bread(dev, SUPERBLOCK_OFF / 1024, 1024, &b) != 0) {
        return 0;
    }
    const struct ext2_super *sb = (const struct ext2_super *)b->data;
    int ok = sb->magic == EXT2_MAGIC && (sb->state & STATE_VALID) &&
             !(sb->state & STATE_ERROR);
    brelse(b);
    return ok;
}

struct fs *ext2_mount(struct blkdev *dev, int ro) {
    struct ext2_fs *e = kmalloc(sizeof(*e));
    if (!e) {
        return nullptr;
    }
    struct buf *b;
    memset(e, 0, sizeof(*e));

    if (bread(dev, SUPERBLOCK_OFF / 1024, 1024, &b) != 0) {
        kfree(e);
        return nullptr;
    }
    memcpy(e->sb_raw, b->data, sizeof(e->sb_raw));
    brelse(b);
    e->sb = (struct ext2_super *)e->sb_raw;

    struct ext2_super *sb = e->sb;
    if (sb->magic != EXT2_MAGIC) {
        kfree(e);
        return nullptr;
    }
    if (sb->rev_level < 1 || (sb->feature_incompat & ~INCOMPAT_FILETYPE)) {
        printf("ext2: %s: unsupported features (rev %lu, incompat %lx)\n",
               dev->name, (unsigned long)sb->rev_level,
               (unsigned long)sb->feature_incompat);
        kfree(e);
        return nullptr;
    }

    e->dev        = dev;
    e->block_size = 1024u << sb->log_block_size;
    e->inode_size = sb->inode_size;
    e->gdt_block  = sb->first_data_block + 1;
    e->groups =
        (sb->blocks_count - sb->first_data_block + sb->blocks_per_group - 1) /
        sb->blocks_per_group;

    e->gdt = kmalloc(e->groups * sizeof(struct ext2_group));
    if (!e->gdt) {
        kfree(e);
        return nullptr;
    }
    for (uint32_t g = 0; g < e->groups; g++) {
        uint32_t off = g * sizeof(struct ext2_group);
        if (read_bytes(e, e->gdt_block + off / e->block_size,
                       off % e->block_size, &e->gdt[g],
                       sizeof(e->gdt[g])) != 0) {
            kfree(e->gdt);
            kfree(e);
            return nullptr;
        }
    }

    int writable = 0;
    (void)ro;
    e->fs = (struct fs){.ops      = &ext2_ops,
                        .vops     = &ext2_vops_ro,
                        .name     = "ext2",
                        .root_ino = EXT2_ROOT_INO,
                        .blk      = dev};

    printf("ext2: %s: %lu blocks of %lu bytes, %lu groups, %s\n", dev->name,
           (unsigned long)sb->blocks_count, (unsigned long)e->block_size,
           (unsigned long)e->groups, writable ? "read-write" : "read-only");
    return &e->fs;
}
