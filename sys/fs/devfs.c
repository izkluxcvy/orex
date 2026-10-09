#include <stddef.h>
#include <stdint.h>

#include <devfs.h>
#include <errno.h>
#include <kmalloc.h>
#include <ktime.h>
#include <memcmp.h>
#include <memcpy.h>
#include <memset.h>
#include <printf.h>
#include <uaccess.h>
#include <vfs.h>

#define ROOT_INO 1

#define DEV(major, minor) (((uint64_t)(major) << 8) | (minor))

static long console_read(struct file *f, void *buf, size_t n) {
    (void)f, (void)buf, (void)n;
    return 0;
}

static long console_write(struct file *f, const void *buf, size_t n) {
    (void)f;
    char   chunk[128];
    size_t done = 0;
    while (done < n) {
        size_t k = n - done < sizeof(chunk) ? n - done : sizeof(chunk);
        if (copy_from_user(chunk, (const char *)buf + done, k) != 0) {
            return done ? (long)done : -EFAULT;
        }
        printf_write(chunk, k);
        done += k;
    }
    return (long)done;
}

static const struct file_ops tty_ops = {.read  = console_read,
                                        .write = console_write};

static int console_open(int flags, struct file **out) {
    return (*out = file_new(&tty_ops, flags, nullptr)) ? 0 : -ENOMEM;
}

struct file *vfs_console() {
    struct file  *f    = file_new(&tty_ops, O_RDWR, nullptr);
    struct vnode *root = vfs_root();
    if (f && vfs_lookup(root, "/dev/console", VFS_FOLLOW, &f->vnode) != 0) {
        f->vnode = nullptr;
    }
    vnode_put(root);
    return f;
}

static long null_read(struct file *f, void *buf, size_t n) {
    (void)f, (void)buf, (void)n;
    return 0;
}

static long sink_write(struct file *f, const void *buf, size_t n) {
    (void)f, (void)buf;
    return (long)n;
}

static long full_write(struct file *f, const void *buf, size_t n) {
    (void)f, (void)buf, (void)n;
    return -ENOSPC;
}

static long fill_read(void *ubuf, size_t n, int random) {
    char   chunk[256];
    size_t done = 0;
    (void)random;
    memset(chunk, 0, sizeof(chunk));
    while (done < n) {
        size_t k = n - done < sizeof(chunk) ? n = done : sizeof(chunk);
        if (copy_to_user((char *)ubuf + done, chunk, k) != 0) {
            return done ? (long)done : -EFAULT;
        }
        done += k;
    }
    return (long)n;
}

static long zero_read(struct file *f, void *buf, size_t n) {
    (void)f;
    return fill_read(buf, n, 0);
}

static long no_size(struct file *f) {
    (void)f;
    return 0;
}

static const struct file_ops null_ops = {
    .read = null_read, .write = sink_write, .size = no_size};
static const struct file_ops zero_ops = {
    .read = zero_read, .write = sink_write, .size = no_size};
static const struct file_ops full_ops = {
    .read = zero_read, .write = full_write, .size = no_size};

static int simple(const struct file_ops *ops, int flags, struct file **out) {
    return (*out = file_new(ops, flags, nullptr)) ? 0 : -ENOMEM;
}

static int null_open(int flags, struct file **out) {
    return simple(&null_ops, flags, out);
}
static int zero_open(int flags, struct file **out) {
    return simple(&zero_ops, flags, out);
}
static int full_open(int flags, struct file **out) {
    return simple(&full_ops, flags, out);
}

struct device {
    const char *name;
    uint32_t    mode;
    uint32_t    gid;
    uint64_t    rdev;
    int (*open)(int flags, struct file **out);
};

static const struct device devices[] = {
    {"console", S_IFCHR | 0620, 5, DEV(5, 1), console_open},
    {"null", S_IFCHR | 0666, 0, DEV(1, 3), null_open},
    {"zero", S_IFCHR | 0666, 0, DEV(1, 5), zero_open},
    {"full", S_IFCHR | 0666, 0, DEV(1, 7), full_open},
};

#define NDEV (sizeof(devices) / sizeof(devices[0]))

static const struct device *dev_of(uint64_t ino) {
    return ino >= 2 && ino - 2 < NDEV ? &devices[ino - 2] : nullptr;
}

static size_t length(const char *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

static int devfs_lookup(struct vnode *dir, const char *name, size_t len,
                        uint64_t *ino) {
    if (len == 2 && name[0] == '.' && name[1] == '.') {
        *ino = ROOT_INO;
        return 0;
    }
    (void)dir;
    for (size_t i = 0; i < NDEV; i++) {
        if (length(devices[i].name) == len &&
            memcmp(devices[i].name, name, len) == 0) {
            *ino = i + 2;
            return 0;
        }
    }
    return -ENOENT;
}

static void set_name(struct vfs_dirent *out, const char *s) {
    size_t n = length(s);
    memcpy(out->name, s, n + 1);
}

static int devfs_readdir(struct vnode *dir, uint64_t *pos,
                         struct vfs_dirent *out) {
    if (*pos < 2) {
        out->ino  = *pos == 0 ? dir->ino : ROOT_INO;
        out->type = DT_DIR;
        set_name(out, *pos == 0 ? "." : "..");
        (*pos)++;
        return 1;
    }
    uint64_t i = *pos - 2;
    if (i < NDEV) {
        out->ino  = i + 2;
        out->type = DT_CHR;
        set_name(out, devices[i].name);
    } else {
        return 0;
    }
    (*pos)++;
    return 1;
}

static uint64_t booted;

static int devfs_vget(struct fs *fs, uint64_t ino, struct vnode *vn) {
    (void)fs;
    vn->atime = vn->mtime = vn->ctime = booted;
    vn->nlink                         = 1;
    if (ino == ROOT_INO) {
        vn->mode  = S_IFDIR | 0755;
        vn->nlink = 2;
        return 0;
    }
    const struct device *d = dev_of(ino);
    if (!d) {
        return -ENOENT;
    }
    vn->mode = d->mode;
    vn->gid  = d->gid;
    vn->rdev = d->rdev;
    return 0;
}

int devfs_open(struct vnode *vn, int flags, struct file **out) {
    const struct device *d   = dev_of(vn->ino);
    int                  err = d ? d->open(flags, out) : -ENOENT;
    if (err) {
        return err;
    }
    (*out)->vnode = vnode_ref(vn);
    return 0;
}

static const struct vnode_ops devfs_vops = {
    .lookup  = devfs_lookup,
    .readdir = devfs_readdir,
};

static const struct fs_ops devfs_ops = {.vget = devfs_vget, .open = devfs_open};

struct fs *devfs_create() {
    struct fs *fs = kmalloc(sizeof(*fs));
    booted        = (uint64_t)(time_real() / NSEC);
    if (fs) {
        *fs = (struct fs){.ops      = &devfs_ops,
                          .vops     = &devfs_vops,
                          .name     = "devfs",
                          .root_ino = ROOT_INO};
    }
    return fs;
}
