#pragma once

#include <stddef.h>
#include <stdint.h>

#define PATH_MAX    4096
#define NAME_MAX    255
#define SYMLOOP_MAX 40

#define S_ISUID     04000
#define S_ISGID     02000
#define S_ISVTX     01000
#define S_IFMT      0170000
#define S_IFIFO     0010000
#define S_IFCHR     0020000
#define S_IFDIR     0040000
#define S_IFBLK     0060000
#define S_IFREG     0100000
#define S_IFLNK     0120000
#define S_IFSOCK    0140000
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12

#define DT_OF(mode) ((uint8_t)(((mode) & S_IFMT) >> 12))

struct fs;
struct vnode;

struct vfs_dirent {
    uint64_t ino;
    uint8_t  type;
    char     name[NAME_MAX + 1];
};

struct vnode_ops {
    int (*lookup)(struct vnode *dir, const char *name, size_t len,
                  uint64_t *ino);
    long (*read)(struct vnode *vn, void *buf, size_t n, uint64_t off);
    int (*readdir)(struct vnode *dir, uint64_t *pos, struct vfs_dirent *out);
    long (*readlink)(struct vnode *vn, char *buf, size_t n);
};

struct fs_ops {
    int (*vget)(struct fs *fs, uint64_t ino, struct vnode *vn);
    int (*vput)(struct vnode *vn);
};

struct fs {
    const struct fs_ops    *ops;
    const struct vnode_ops *vops;
    const char             *name;
    uint64_t                root_ino;
    uint32_t                dev;
    void                   *priv;
};

struct mount;

struct vnode {
    struct fs    *fs;
    uint64_t      ino;
    uint32_t      mode;
    uint32_t      uid, gid;
    uint32_t      nlink;
    uint64_t      rdev;
    uint64_t      size;
    uint64_t      atime, mtime, ctime;
    void         *priv;
    int           refs;
    struct mount *covered_by;
    struct mount *root_of;
    struct vnode *next;
};

int           vnode_get(struct fs *fs, uint64_t ino, struct vnode **out);
struct vnode *vnode_ref(struct vnode *vn);
void          vnode_put(struct vnode *vn);
long          vnode_read(struct vnode *vn, void *buf, size_t n, uint64_t off);

int           vfs_mount_root(struct fs *fs);
int           vfs_mount(const char *path, struct fs *fs);
struct vnode *vfs_root();

#define VFS_FOLLOW 1

int vfs_lookup(struct vnode *base, const char *path, int flags,
               struct vnode **out);
