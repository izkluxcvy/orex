#include <stddef.h>
#include <stdint.h>

#include <blk.h>
#include <errno.h>
#include <ext2.h>
#include <kmalloc.h>
#include <ktime.h>
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
#define RO_COMPAT_SPARSE     0x0001
#define RO_COMPAT_LARGE_FILE 0x0002
#define RO_COMPAT_BTREE_DIR  0x0004
#define RO_COMPAT_WRITABLE                                                     \
    (RO_COMPAT_SPARSE | RO_COMPAT_LARGE_FILE | RO_COMPAT_BTREE_DIR)

#define INDEX_FL 0x1000

#define FT_REG     1
#define FT_DIR     2
#define FT_SYMLINK 7

#define FAST_SYMLINK_MAX 60
#define DIR_LINK_MAX     32000

static uint8_t ft_of(uint32_t mode) {
    static const uint8_t ft[16] = {[1] = 5, [2] = 3,  [4] = 2, [6] = 4,
                                   [8] = 1, [10] = 7, [12] = 6};
    return ft[(mode >> 12) & 15];
}

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
    int      can_write;
};

static struct ext2_fs *efs_of(struct vnode *vn) {
    return (struct ext2_fs *)vn->fs;
}

static struct ext2_inode *inode_of(struct vnode *vn) { return vn->priv; }

static uint32_t sectors_per_block(struct ext2_fs *e) {
    return e->block_size / 512;
}

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

static int write_bytes(struct ext2_fs *e, uint64_t block, uint32_t off,
                       const void *src, size_t len) {
    struct buf *b;
    int         whole = off == 0 && len == e->block_size;
    int         err   = whole ? bget(e->dev, block, e->block_size, &b)
                              : bread(e->dev, block, e->block_size, &b);
    if (err) {
        return err;
    }
    if (src) {
        memcpy(b->data + off, src, len);
    } else {
        memset(b->data + off, 0, len);
    }
    err = bwrite(b);
    brelse(b);
    return err;
}

static int sync_super(struct ext2_fs *e) {
    struct buf *b;
    int         err = bread(e->dev, SUPERBLOCK_OFF / 1024, 1024, &b);
    if (err) {
        return err;
    }
    memcpy(b->data, e->sb_raw, sizeof(e->sb_raw));
    err = bwrite(b);
    brelse(b);
    return err;
}

static int sync_group(struct ext2_fs *e, uint32_t g) {
    uint32_t off = g * sizeof(struct ext2_group);
    return write_bytes(e, e->gdt_block + off / e->block_size,
                       off % e->block_size, &e->gdt[g], sizeof(e->gdt[g]));
}

// --- inodes ------------------------------------------------------------------

static int inode_io(struct ext2_fs *e, uint32_t ino, struct ext2_inode *ip,
                    int write) {
    uint32_t group = (ino - 1) / e->sb->inodes_per_group;
    uint32_t index = (ino - 1) % e->sb->inodes_per_group;
    if (ino == 0 || group >= e->groups) {
        return -ENOENT;
    }

    uint64_t off   = (uint64_t)index * e->inode_size;
    uint64_t block = e->gdt[group].inode_table + off / e->block_size;
    uint32_t boff  = (uint32_t)(off % e->block_size);
    return write ? write_bytes(e, block, boff, ip, sizeof(*ip))
                 : read_bytes(e, block, boff, ip, sizeof(*ip));
}

static uint64_t file_size(struct ext2_fs *e, struct ext2_inode *ip) {
    uint64_t size = ip->size;
    if ((ip->mode & S_IFMT) == S_IFREG &&
        (e->sb->feature_ro_compat & RO_COMPAT_LARGE_FILE)) {
        size |= (uint64_t)ip->size_high << 32;
    }
    return size;
}

static int sync_vnode(struct vnode *vn) {
    struct ext2_fs    *e  = efs_of(vn);
    struct ext2_inode *ip = inode_of(vn);

    ip->size = (uint32_t)vn->size;
    if (S_ISREG(vn->mode)) {
        ip->size_high = (uint32_t)(vn->size >> 32);
    }
    ip->links_count = (uint16_t)vn->nlink;
    ip->mode        = (uint16_t)vn->mode;
    ip->uid         = (uint16_t)vn->uid;
    ip->uid_high    = (uint16_t)(vn->uid >> 16);
    ip->gid         = (uint16_t)vn->gid;
    ip->gid_high    = (uint16_t)(vn->gid >> 16);
    ip->atime       = (uint32_t)vn->atime;
    ip->mtime       = (uint32_t)vn->mtime;
    ip->ctime       = (uint32_t)vn->ctime;
    return inode_io(e, vn->ino, ip, 1);
}

// --- allocation --------------------------------------------------------------

static int bitmap_take(struct ext2_fs *e, uint32_t bitmap, uint32_t limit,
                       uint32_t *bit) {
    struct buf *b;
    int         err = bread(e->dev, bitmap, e->block_size, &b);
    if (err) {
        return err;
    }
    err = -ENOSPC;
    for (uint32_t i = 0; i < limit; i++) {
        if (!(b->data[i / 8] & (1u << (i % 8)))) {
            b->data[i / 8] |= (uint8_t)(1u << (i % 8));
            *bit = i;
            err  = bwrite(b);
            break;
        }
    }
    brelse(b);
    return err;
}

static int bitmap_clear(struct ext2_fs *e, uint32_t bitmap, uint32_t bit) {
    struct buf *b;
    int         err = bread(e->dev, bitmap, e->block_size, &b);
    if (err) {
        return err;
    }
    b->data[bit / 8] &= (uint8_t)~(1u << (bit % 8));
    err = bwrite(b);
    brelse(b);
    return err;
}

static int block_alloc(struct ext2_fs *e, struct ext2_inode *ip,
                       uint32_t *out) {
    uint32_t bpg = e->sb->blocks_per_group;

    for (uint32_t g = 0; g < e->groups; g++) {
        if (!e->gdt[g].free_blocks) {
            continue;
        }
        uint32_t first = e->sb->first_data_block + g * bpg;
        uint32_t limit = e->sb->blocks_count - first < bpg
                             ? e->sb->blocks_count - first
                             : bpg;
        uint32_t bit;
        int      err = bitmap_take(e, e->gdt[g].block_bitmap, limit, &bit);
        if (err == -ENOSPC) {
            continue;
        }
        if (err) {
            return err;
        }

        e->gdt[g].free_blocks--;
        e->sb->free_blocks_count--;
        *out = first + bit;
        ip->blocks += sectors_per_block(e);
        if ((err = sync_group(e, g)) != 0 || (err = sync_super(e)) != 0) {
            return err;
        }
        return write_bytes(e, *out, 0, nullptr, e->block_size);
    }
    return -ENOSPC;
}

static int block_free(struct ext2_fs *e, struct ext2_inode *ip,
                      uint32_t block) {
    bforget(e->dev, block, e->block_size);
    uint32_t rel = block - e->sb->first_data_block;
    uint32_t g   = rel / e->sb->blocks_per_group;
    int      err =
        bitmap_clear(e, e->gdt[g].block_bitmap, rel % e->sb->blocks_per_group);
    if (err) {
        return err;
    }
    e->gdt[g].free_blocks++;
    e->sb->free_blocks_count++;
    ip->blocks -= sectors_per_block(e);
    if ((err = sync_group(e, g)) != 0) {
        return err;
    }
    return sync_super(e);
}

static int inode_alloc(struct ext2_fs *e, int dir, uint32_t *ino) {
    uint32_t ipg = e->sb->inodes_per_group;

    for (uint32_t g = 0; g < e->groups; g++) {
        if (!e->gdt[g].free_inodes) {
            continue;
        }
        uint32_t bit;
        int      err = bitmap_take(e, e->gdt[g].inode_bitmap, ipg, &bit);
        if (err == -ENOSPC) {
            continue;
        }
        if (err) {
            return err;
        }

        e->gdt[g].free_inodes--;
        e->sb->free_inodes_count--;
        if (dir) {
            e->gdt[g].used_dirs++;
        }
        *ino = g * ipg + bit + 1;
        if ((err = sync_group(e, g)) != 0 || (err = sync_super(e)) != 0) {
            return err;
        }

        uint64_t off = (uint64_t)bit * e->inode_size;
        return write_bytes(e, e->gdt[g].inode_table + off / e->block_size,
                           (uint32_t)(off % e->block_size), nullptr,
                           e->inode_size);
    }
    return -ENOSPC;
}

static int inode_free(struct ext2_fs *e, uint32_t ino, int dir) {
    uint32_t g   = (ino - 1) / e->sb->inodes_per_group;
    int      err = bitmap_clear(e, e->gdt[g].inode_bitmap,
                                (ino - 1) % e->sb->inodes_per_group);
    if (err) {
        return err;
    }
    e->gdt[g].free_inodes++;
    e->sb->free_inodes_count++;
    if (dir) {
        e->gdt[g].used_dirs--;
    }
    if ((err = sync_group(e, g)) != 0) {
        return err;
    }
    return sync_super(e);
}

// --- block mapping -----------------------------------------------------------

static int bmap(struct ext2_fs *e, struct ext2_inode *ip, uint64_t fblock,
                int alloc, uint32_t *out) {
    uint32_t per = e->block_size / 4;
    int      err;

    if (fblock < NDIRECT) {
        if (!ip->block[fblock] && alloc &&
            (err = block_alloc(e, ip, &ip->block[fblock])) != 0) {
            return err;
        }
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

    uint32_t *slot = &ip->block[NDIRECT + level - 1];
    if (!*slot && alloc && (err = block_alloc(e, ip, slot)) != 0) {
        return err;
    }
    uint32_t block = *slot;

    for (; level > 0 && block; level--) {
        span /= per;
        uint32_t idx = (uint32_t)(fblock / span);
        fblock %= span;

        uint32_t next;
        if ((err = read_bytes(e, block, idx * 4, &next, 4)) != 0) {
            return err;
        }
        if (!next && alloc) {
            if ((err = block_alloc(e, ip, &next)) != 0 ||
                (err = write_bytes(e, block, idx * 4, &next, 4)) != 0) {
                return err;
            }
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

static int free_tree(struct ext2_fs *e, struct ext2_inode *ip, uint32_t block,
                     int level) {
    if (!block) {
        return 0;
    }
    if (level > 0) {
        uint32_t *ptrs = kmalloc(e->block_size);
        if (!ptrs) {
            return -ENOMEM;
        }
        int err = read_bytes(e, block, 0, ptrs, e->block_size);
        for (uint32_t i = 0; !err && i < e->block_size / 4; i++) {
            err = free_tree(e, ip, ptrs[i], level - 1);
        }
        kfree(ptrs);
        if (err) {
            return err;
        }
    }
    return block_free(e, ip, block);
}

static int trim_tree(struct ext2_fs *e, struct ext2_inode *ip, uint32_t *slot,
                     int level, uint64_t base, uint64_t keep) {
    if (!*slot) {
        return 0;
    }
    if (base >= keep) {
        int err = free_tree(e, ip, *slot, level);
        *slot   = 0;
        return err;
    }
    if (level == 0) {
        return 0;
    }
    uint32_t per  = e->block_size / 4;
    uint64_t span = 1;
    for (int i = 1; i < level; i++) {
        span *= per;
    }
    uint32_t *ptrs = kmalloc(e->block_size);
    if (!ptrs) {
        return -ENOMEM;
    }
    int err = read_bytes(e, *slot, 0, ptrs, e->block_size);
    for (uint32_t i = 0; !err && i < per; i++) {
        if (base + (i + 1) * span > keep) {
            err = trim_tree(e, ip, &ptrs[i], level - 1, base + i * span, keep);
        }
    }
    if (!err) {
        err = write_bytes(e, *slot, 0, ptrs, e->block_size);
    }
    kfree(ptrs);
    return err;
}

static int truncate_locked(struct vnode *vn, uint64_t size) {
    struct ext2_fs    *e   = efs_of(vn);
    struct ext2_inode *ip  = inode_of(vn);
    int                err = 0;

    if (!has_blocks(vn)) {
        if (size == 0) {
            memset(ip->block, 0, sizeof(ip->block));
        }
    } else if (size < vn->size) {
        uint64_t keep = (size + e->block_size - 1) / e->block_size;
        uint32_t per  = e->block_size / 4;
        uint64_t base = NDIRECT;
        uint64_t span = per;
        for (int i = 0; i < NDIRECT && !err; i++) {
            err = trim_tree(e, ip, &ip->block[i], 0, (uint64_t)i, keep);
        }
        for (int level = 1; level <= 3 && !err; level++) {
            err = trim_tree(e, ip, &ip->block[NDIRECT + level - 1], level, base,
                            keep);
            base += span;
            span *= per;
        }
        uint32_t tail  = (uint32_t)(size % e->block_size);
        uint32_t block = 0;
        if (!err && tail &&
            (err = bmap(e, ip, size / e->block_size, 0, &block)) == 0 &&
            block) {
            err = write_bytes(e, block, tail, nullptr, e->block_size - tail);
        }
    }
    vn->size = size;
    int serr = sync_vnode(vn);
    return err ? err : serr;
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

static long write_locked(struct vnode *vn, const void *buf, size_t n,
                         uint64_t off) {
    struct ext2_fs    *e  = efs_of(vn);
    struct ext2_inode *ip = inode_of(vn);
    const uint8_t     *in = buf;

    uint64_t max = (e->sb->feature_ro_compat & RO_COMPAT_LARGE_FILE)
                       ? (uint64_t)1 << 40
                       : 0x7fff'ffffULL;
    if (off >= max) {
        return -EFBIG;
    }
    if (n > max - off) {
        n = max - off;
    }

    size_t done = 0;
    int    err  = 0;
    while (done < n) {
        uint64_t fblock = (off + done) / e->block_size;
        uint32_t boff   = (uint32_t)((off + done) % e->block_size);
        size_t   chunk  = e->block_size - boff;
        if (chunk > n - done) {
            chunk = n - done;
        }

        uint32_t block;
        if ((err = bmap(e, ip, fblock, 1, &block)) != 0 ||
            (err = write_bytes(e, block, boff, in + done, chunk)) != 0) {
            break;
        }
        done += chunk;
        if (off + done > vn->size) {
            vn->size = off + done;
        }
    }

    int serr = sync_vnode(vn);
    if (done) {
        return (long)done;
    }
    return err ? err : serr;
}

// --- directories -------------------------------------------------------------

static size_t dirent_len(size_t name_len) {
    return (8 + name_len + 3) & ~(size_t)3;
}

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

static int add_entry(struct vnode *dir, const char *name, size_t len,
                     uint32_t ino, uint8_t type) {
    struct ext2_fs    *e    = efs_of(dir);
    struct ext2_inode *ip   = inode_of(dir);
    size_t             need = dirent_len(len);
    int                err;

    ip->flags &= ~INDEX_FL;

    uint64_t nblocks = dir->size / e->block_size;
    for (uint64_t fb = 0; fb <= nblocks; fb++) {
        int      grow = (fb == nblocks);
        uint32_t block;
        if ((err = bmap(e, ip, fb, grow, &block)) != 0) {
            return err;
        }
        if (!block) {
            continue;
        }

        struct buf *b;
        if ((err = bread(e->dev, block, e->block_size, &b)) != 0) {
            return err;
        }

        struct ext2_dirent *slot = nullptr;
        if (grow) {
            slot          = (struct ext2_dirent *)b->data;
            slot->rec_len = (uint16_t)e->block_size;
            dir->size += e->block_size;
        }
        for (uint32_t off = 0; !slot && off < e->block_size;) {
            struct ext2_dirent *h = (struct ext2_dirent *)(b->data + off);
            if (h->rec_len == 0) {
                break;
            }
            size_t used = h->inode ? dirent_len(h->name_len) : 0;
            if (h->rec_len - used >= need) {
                if (used) {
                    struct ext2_dirent *n =
                        (struct ext2_dirent *)(b->data + off + used);
                    n->rec_len = (uint16_t)(h->rec_len - used);
                    h->rec_len = (uint16_t)used;
                    h          = n;
                }
                slot = h;
            }
            off += h->rec_len;
        }

        if (slot) {
            slot->inode     = ino;
            slot->name_len  = (uint8_t)len;
            slot->file_type = type;
            memcpy(slot->name, name, len);
            err = bwrite(b);
            brelse(b);
            int serr = sync_vnode(dir);
            return err ? err : serr;
        }
        brelse(b);
    }
    return -ENOSPC;
}

static int remove_entry(struct vnode *dir, const char *name, size_t len) {
    struct ext2_fs    *e  = efs_of(dir);
    struct ext2_inode *ip = inode_of(dir);

    ip->flags &= ~INDEX_FL;

    for (uint64_t fb = 0; fb < dir->size / e->block_size; fb++) {
        uint32_t block;
        int      err = bmap(e, ip, fb, 0, &block);
        if (err) {
            return err;
        }
        if (!block) {
            continue;
        }
        struct buf *b;
        if ((err = bread(e->dev, block, e->block_size, &b)) != 0) {
            return err;
        }

        struct ext2_dirent *prev = nullptr;
        for (uint32_t off = 0; off < e->block_size;) {
            struct ext2_dirent *h = (struct ext2_dirent *)(b->data + off);
            if (h->rec_len == 0) {
                break;
            }
            if (h->inode && h->name_len == len &&
                memcmp(h->name, name, len) == 0) {
                if (prev) {
                    prev->rec_len += h->rec_len;
                } else {
                    h->inode = 0;
                }
                err = bwrite(b);
                brelse(b);
                int serr = sync_vnode(dir);
                return err ? err : serr;
            }
            prev = h;
            off += h->rec_len;
        }
        brelse(b);
    }
    return -ENOENT;
}

static int retarget_entry(struct vnode *dir, const char *name, size_t len,
                          uint32_t ino, uint8_t type) {
    struct ext2_fs    *e  = efs_of(dir);
    struct ext2_inode *ip = inode_of(dir);

    for (uint64_t fb = 0; fb < dir->size / e->block_size; fb++) {
        uint32_t block;
        int      err = bmap(e, ip, fb, 0, &block);
        if (err) {
            return err;
        }
        if (!block) {
            continue;
        }
        struct buf *b;
        if ((err = bread(e->dev, block, e->block_size, &b)) != 0) {
            return err;
        }
        for (uint32_t off = 0; off < e->block_size;) {
            struct ext2_dirent *h = (struct ext2_dirent *)(b->data + off);
            if (h->rec_len == 0) {
                break;
            }
            if (h->inode && h->name_len == len &&
                memcmp(h->name, name, len) == 0) {
                h->inode     = ino;
                h->file_type = type;
                err          = bwrite(b);
                brelse(b);
                return err;
            }
            off += h->rec_len;
        }
        brelse(b);
    }
    return -ENOENT;
}

static int dir_is_empty(struct vnode *dir) {
    struct vfs_dirent *de = kmalloc(sizeof(*de));
    if (!de) {
        return -ENOMEM;
    }
    uint64_t pos   = 0;
    int      got   = 0;
    int      empty = 1;
    while (empty && (got = next_dirent(dir, &pos, de)) > 0) {
        empty =
            de->name[0] == '.' && (de->name[1] == '\0' ||
                                   (de->name[1] == '.' && de->name[2] == '\0'));
    }
    kfree(de);
    return got < 0 ? got : empty;
}

static int make_inode(struct ext2_fs *e, uint32_t mode, uint16_t links,
                      uint32_t uid, uint32_t gid, uint32_t *ino,
                      struct ext2_inode *ip) {
    int err = inode_alloc(e, S_ISDIR(mode), ino);
    if (err) {
        return err;
    }
    memset(ip, 0, sizeof(*ip));
    ip->mode        = (uint16_t)mode;
    ip->links_count = links;
    ip->uid         = (uint16_t)uid;
    ip->uid_high    = (uint16_t)(uid >> 16);
    ip->gid         = (uint16_t)gid;
    ip->gid_high    = (uint16_t)(gid >> 16);
    ip->atime = ip->mtime = ip->ctime = (uint32_t)(time_real() / NSEC);
    return 0;
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

static long ext2_write(struct vnode *vn, const void *buf, size_t n,
                       uint64_t off) {
    struct ext2_fs *e = efs_of(vn);
    mutex_lock(&e->lock);
    long done = write_locked(vn, buf, n, off);
    mutex_unlock(&e->lock);
    return done;
}

static int ext2_truncate(struct vnode *vn, uint64_t size) {
    struct ext2_fs *e   = efs_of(vn);
    uint64_t        max = (e->sb->feature_ro_compat & RO_COMPAT_LARGE_FILE)
                              ? (uint64_t)1 << 40
                              : 0x7fff'ffffULL;
    if (size > max) {
        return -EFBIG;
    }
    mutex_lock(&e->lock);
    int err = truncate_locked(vn, size);
    mutex_unlock(&e->lock);
    return err;
}

static void set_rdev(struct ext2_inode *ip, uint64_t rdev) {
    uint32_t major = (uint32_t)(rdev >> 8);
    uint32_t minor = (uint32_t)(rdev & 0xff);
    if (major < 256) {
        ip->block[0] = major << 8 | minor;
    } else {
        ip->block[1] = (minor & 0xff) | (major << 8) | ((minor & ~0xffu) << 12);
    }
}

static uint64_t get_rdev(const struct ext2_inode *ip) {
    if (ip->block[0]) {
        return ip->block[0] & 0xffff;
    }
    uint32_t v = ip->block[1];
    return (uint64_t)((v >> 8) & 0xfff) << 8 |
           ((v & 0xff) | ((v >> 12) & ~0xffu));
}

static int new_entry(struct vnode *dir, const char *name, size_t len,
                     struct ext2_inode *ip, uint32_t ino, uint64_t *out) {
    int err = inode_io(efs_of(dir), ino, ip, 1);
    if (!err) {
        err = add_entry(dir, name, len, ino, ft_of(ip->mode));
    }
    if (!err && out) {
        *out = ino;
    }
    return err;
}

static int ext2_create(struct vnode *dir, const char *name, size_t len,
                       uint32_t mode, uint64_t rdev, uint32_t uid, uint32_t gid,
                       uint64_t *out) {
    struct ext2_fs   *e = efs_of(dir);
    struct ext2_inode ip;
    uint32_t          ino;
    uint64_t          existing;

    mutex_lock(&e->lock);
    int err = lookup_locked(dir, name, len, &existing);
    if (err == 0) {
        err = -EEXIST;
    } else if (err == -ENOENT &&
               (err = make_inode(e, mode, 1, uid, gid, &ino, &ip)) == 0) {
        if (S_ISCHR(mode) || (mode & S_IFMT) == S_IFBLK) {
            set_rdev(&ip, rdev);
        }
        err = new_entry(dir, name, len, &ip, ino, out);
    }
    mutex_unlock(&e->lock);
    return err;
}

static int ext2_symlink(struct vnode *dir, const char *name, size_t len,
                        const char *target, size_t tlen, uint32_t uid,
                        uint32_t gid) {
    struct ext2_fs   *e = efs_of(dir);
    struct ext2_inode ip;
    uint32_t          ino;
    uint64_t          existing;
    if (tlen >= e->block_size) {
        return -ENAMETOOLONG;
    }

    mutex_lock(&e->lock);
    int err = lookup_locked(dir, name, len, &existing);
    if (err == 0) {
        err = -EEXIST;
    } else if (err == -ENOENT && (err = make_inode(e, S_IFLNK | 0777, 1, uid,
                                                   gid, &ino, &ip)) == 0) {
        ip.size = (uint32_t)tlen;
        if (tlen < FAST_SYMLINK_MAX) {
            memcpy(ip.block, target, tlen);
        } else {
            err = write_bytes(e, ip.block[0], 0, target, tlen);
        }
        if (!err) {
            err = new_entry(dir, name, len, &ip, ino, nullptr);
        }
    }
    mutex_unlock(&e->lock);
    return err;
}

static int ext2_link(struct vnode *dir, const char *name, size_t len,
                     struct vnode *vn) {
    struct ext2_fs *e = efs_of(dir);
    uint64_t        existing;
    mutex_lock(&e->lock);
    int err = lookup_locked(dir, name, len, &existing);
    if (err == 0) {
        err = -EEXIST;
    } else if (err == -ENOENT &&
               (err = add_entry(dir, name, len, (uint32_t)vn->ino,
                                ft_of(vn->mode))) == 0) {
        vn->nlink++;
        err = sync_vnode(vn);
    }
    mutex_unlock(&e->lock);
    return err;
}

static int ext2_rename(struct vnode *odir, const char *oname, size_t olen,
                       struct vnode *ndir, const char *nname, size_t nlen,
                       struct vnode *moved, struct vnode *victim) {
    struct ext2_fs *e      = efs_of(odir);
    int             is_dir = S_ISDIR(moved->mode);
    int             err    = 0;
    mutex_lock(&e->lock);
    if (victim && S_ISDIR(victim->mode)) {
        err = dir_is_empty(victim);
        err = err < 0 ? err : err ? 0 : -ENOTEMPTY;
    }
    if (!err && is_dir && odir != ndir && !victim &&
        ndir->nlink >= DIR_LINK_MAX) {
        err = -EMLINK;
    }
    if (!err) {
        err = victim ? retarget_entry(ndir, nname, nlen, (uint32_t)moved->ino,
                                      ft_of(moved->mode))
                     : add_entry(ndir, nname, nlen, (uint32_t)moved->ino,
                                 ft_of(moved->mode));
    }
    if (!err && victim) {
        if (S_ISDIR(victim->mode)) {
            victim->nlink = 0;
            ndir->nlink--;
        } else {
            victim->nlink--;
        }
        err = sync_vnode(victim);
    }
    if (!err) {
        err = remove_entry(odir, oname, olen);
    }
    if (!err && is_dir && odir != ndir) {
        err = retarget_entry(moved, "..", 2, (uint32_t)ndir->ino, FT_DIR);
        odir->nlink--;
        ndir->nlink++;
    }
    if (!err) {
        int e1 = sync_vnode(odir);
        int e2 = sync_vnode(ndir);
        int e3 = sync_vnode(moved);
        err    = e1 ? e1 : e2 ? e2 : e3;
    }
    mutex_unlock(&e->lock);
    return err;
}

static int ext2_mkdir(struct vnode *dir, const char *name, size_t len,
                      uint32_t mode, uint32_t uid, uint32_t gid) {
    struct ext2_fs   *e = efs_of(dir);
    struct ext2_inode ip;
    uint32_t          ino, block;
    uint64_t          existing;

    mutex_lock(&e->lock);
    int err = lookup_locked(dir, name, len, &existing);
    if (err == 0) {
        err = -EEXIST;
    } else if (err == -ENOENT && dir->nlink >= DIR_LINK_MAX) {
        err = -EMLINK;
    } else if (err == -ENOENT &&
               (err = make_inode(e, mode, 2, uid, gid, &ino, &ip)) == 0 &&
               (err = block_alloc(e, &ip, &block)) == 0) {
        uint8_t             ents[24] = {0};
        struct ext2_dirent *dot      = (struct ext2_dirent *)ents;
        struct ext2_dirent *dotdot   = (struct ext2_dirent *)(ents + 12);
        *dot                         = (struct ext2_dirent){ino, 12, 1, FT_DIR};
        *dotdot                      = (struct ext2_dirent){
            (uint32_t)dir->ino, (uint16_t)(e->block_size - 12), 2, FT_DIR};
        dot->name[0]    = '.';
        dotdot->name[0] = dotdot->name[1] = '.';

        ip.block[0] = block;
        ip.size     = e->block_size;
        if ((err = write_bytes(e, block, 0, ents, sizeof(ents))) == 0 &&
            (err = inode_io(e, ino, &ip, 1)) == 0 &&
            (err = add_entry(dir, name, len, ino, FT_DIR)) == 0) {
            dir->nlink++;
            err = sync_vnode(dir);
        }
    }
    mutex_unlock(&e->lock);
    return err;
}

static int ext2_unlink(struct vnode *dir, const char *name, size_t len,
                       struct vnode *victim) {
    struct ext2_fs *e = efs_of(dir);
    mutex_lock(&e->lock);
    int err = remove_entry(dir, name, len);
    if (!err) {
        victim->nlink--;
        err = sync_vnode(victim);
    }
    mutex_unlock(&e->lock);
    return err;
}

static int ext2_rmdir(struct vnode *dir, const char *name, size_t len,
                      struct vnode *victim) {
    struct ext2_fs *e = efs_of(dir);
    mutex_lock(&e->lock);
    int err = dir_is_empty(victim);
    if (err == 0) {
        err = -ENOTEMPTY;
    } else if (err > 0 && (err = remove_entry(dir, name, len)) == 0) {
        victim->nlink = 0;
        dir->nlink--;
        if ((err = sync_vnode(victim)) == 0) {
            err = sync_vnode(dir);
        }
    }
    mutex_unlock(&e->lock);
    return err;
}

static int ext2_setattr(struct vnode *vn) {
    struct ext2_fs *e = efs_of(vn);
    mutex_lock(&e->lock);
    int err = sync_vnode(vn);
    mutex_unlock(&e->lock);
    return err;
}

static int ext2_vget(struct fs *fs, uint64_t ino, struct vnode *vn) {
    struct ext2_fs    *e  = (struct ext2_fs *)fs;
    struct ext2_inode *ip = kmalloc(sizeof(*ip));
    if (!ip) {
        return -ENOMEM;
    }
    mutex_lock(&e->lock);
    int err = inode_io(e, (uint32_t)ino, ip, 0);
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
    struct ext2_fs    *e  = efs_of(vn);
    struct ext2_inode *ip = inode_of(vn);

    if (vn->nlink == 0 && vn->fs->vops->write) {
        mutex_lock(&e->lock);
        truncate_locked(vn, 0);
        ip->dtime = e->sb->wtime > e->sb->inodes_count
                        ? e->sb->wtime
                        : e->sb->inodes_count + 1;
        inode_io(e, (uint32_t)vn->ino, ip, 1);
        inode_free(e, (uint32_t)vn->ino, S_ISDIR(vn->mode));
        mutex_unlock(&e->lock);
    }
    kfree(ip);
}

static const struct vnode_ops ext2_vops_rw = {
    .lookup   = ext2_lookup,
    .read     = ext2_read,
    .readdir  = ext2_readdir,
    .readlink = ext2_readlink,
    .write    = ext2_write,
    .truncate = ext2_truncate,
    .create   = ext2_create,
    .mkdir    = ext2_mkdir,
    .symlink  = ext2_symlink,
    .link     = ext2_link,
    .unlink   = ext2_unlink,
    .rmdir    = ext2_rmdir,
    .rename   = ext2_rename,
    .setattr  = ext2_setattr,
};

static const struct vnode_ops ext2_vops_ro = {
    .lookup   = ext2_lookup,
    .read     = ext2_read,
    .readdir  = ext2_readdir,
    .readlink = ext2_readlink,
};

static void mark_mounted(struct ext2_fs *e) {
    struct ext2_super *sb = e->sb;
    if (!(sb->state & STATE_VALID) || (sb->state & STATE_ERROR)) {
        printf("ext2: %s: not cleanly unmounted\n", e->dev->name);
    }
    sb->state &= ~STATE_VALID;
    sb->mnt_count++;
    sb->mtime = (uint32_t)(time_real() / NSEC);
    sync_super(e);
}

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
    printf("ext2: sb->state = 0x%x, %s\n", sb->state,
           ok ? "clean" : "not clean");
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

    e->can_write = !(sb->feature_ro_compat & ~RO_COMPAT_WRITABLE);
    int writable = e->can_write && !ro;
    e->fs        = (struct fs){
        .ops      = &ext2_ops,
        .vops     = writable ? &ext2_vops_rw : &ext2_vops_ro,
        .name     = "ext2",
        .root_ino = EXT2_ROOT_INO,
        .blk      = dev,
    };

    printf("ext2: %s: %lu blocks of %lu bytes, %lu groups, %s\n", dev->name,
           (unsigned long)sb->blocks_count, (unsigned long)e->block_size,
           (unsigned long)e->groups, writable ? "read-write" : "read-only");
    if (writable) {
        mark_mounted(e);
    }
    return &e->fs;
}
