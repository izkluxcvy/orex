#pragma once

#include <stddef.h>
#include <stdint.h>

#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_ACCMODE   3
#define O_CREAT     0100
#define O_EXCL      0200
#define O_TRUNC     01000
#define O_APPEND    02000
#define O_NOCTTY    0400
#define O_NONBLOCK  04000
#define O_DSYNC     010000
#define O_LARGEFILE 0100000
#define O_DIRECTORY 0200000
#define O_NOFOLLOW  0400000
#define O_CLOEXEC   02000000
#define O_SYNC      04010000
#define O_PATH      010000000
#define O_CLOFORK   040000000

#define O_STATUS   (O_ACCMODE | O_APPEND | O_NONBLOCK | O_SYNC | O_PATH)
#define O_SETTABLE (O_APPEND | O_NONBLOCK)

#define AT_FDCWD            (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR        0x200
#define AT_EACCESS          0x200
#define AT_EMPTY_PATH       0x1000

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define R_OK 4
#define W_OK 2
#define X_OK 1

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

struct file;
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
    int (*open)(struct vnode *vn, int flags, struct file **out);
};

struct fs {
    const struct fs_ops    *ops;
    const struct vnode_ops *vops;
    const char             *name;
    uint64_t                root_ino;
    uint32_t                dev;
    void                   *priv;
};

struct file_ops {
    long (*read)(struct file *f, void *buf, size_t n);
    long (*write)(struct file *f, const void *buf, size_t n);
    long (*size)(struct file *f);
    long (*ioctl)(struct file *f, unsigned long req, void *arg);
    long (*poll)(struct file *f);
    long (*close)(struct file *f);
};

struct file {
    const struct file_ops *ops;
    int                    refs;
    int                    flags;
    long                   offset;
    void                  *data;
    struct vnode          *vnode;
    int                    owner;
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
long vnode_read_user(struct vnode *vn, void *ubuf, size_t n, uint64_t off);

int           vfs_mount_root(struct fs *fs);
int           vfs_mount(const char *path, struct fs *fs);
struct vnode *vfs_root();

#define VFS_FOLLOW 1

int vfs_lookup(struct vnode *base, const char *path, int flags,
               struct vnode **out);
int vfs_open(struct vnode *base, const char *path, int flags, uint32_t mode,
             struct file **out);
int vfs_open_vnode(struct vnode *vn, int flags, struct file **out);

int  vfs_chdir(struct vnode *dir);
long vfs_getcwd(char *buf, size_t size);
int  vfs_exec_open(const char *path, struct vnode **out);
int  vfs_exec_check(struct vnode *vn);

long vfs_lseek(struct file *f, long offset, int whence);

struct stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t pad0;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    uint64_t st_atime, st_atime_nsec;
    uint64_t st_mtime, st_mtime_nsec;
    uint64_t st_ctime, st_ctime_nsec;
    int64_t  unused[3];
};

void vfs_stat(struct vnode *vn, struct stat *st);

struct file *vfs_console();

struct file *file_new(const struct file_ops *ops, int flags, void *data);
struct file *file_ref(struct file *f);
void         file_unref(struct file *f);
