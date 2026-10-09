#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <kmalloc.h>
#include <memcpy.h>
#include <memmove.h>
#include <memset.h>
#include <proc.h>
#include <spinlock.h>
#include <uaccess.h>
#include <vfs.h>

struct mount {
    struct fs    *fs;
    struct vnode *root;
    struct vnode *covered;
    char          path[64];
    struct mount *next;
};

static struct mount *mount_list;

static struct spinlock file_lock  = {.name = "file"};
static struct spinlock vnode_lock = {.name = "vnode"};

static struct vnode *vnodes;
static struct mount *root_mount;
static uint32_t      next_dev = 1;

struct file *file_new(const struct file_ops *ops, int flags, void *data) {
    struct file *f = kmalloc(sizeof(*f));
    if (f) {
        *f = (struct file){.ops = ops, .refs = 1, .flags = flags, .data = data};
    }
    return f;
}

struct file *file_ref(struct file *f) {
    uint64_t flags = spin_lock_irqsave(&file_lock);
    f->refs++;
    spin_unlock_irqrestore(&file_lock, flags);
    return f;
}

void file_unref(struct file *f) {
    uint64_t flags = spin_lock_irqsave(&file_lock);
    int      last  = --f->refs == 0;
    spin_unlock_irqrestore(&file_lock, flags);

    if (last) {
        if (f->ops->close) {
            f->ops->close(f);
        }
        if (f->vnode) {
            vnode_put(f->vnode);
        }
        kfree(f);
    }
}

static size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

static struct vnode *cache_find(struct fs *fs, uint64_t ino) {
    for (struct vnode *vn = vnodes; vn; vn = vn->next) {
        if (vn->fs == fs && vn->ino == ino) {
            vn->refs++;
            return vn;
        }
    }
    return nullptr;
}

int vnode_get(struct fs *fs, uint64_t ino, struct vnode **out) {
    uint64_t flags = spin_lock_irqsave(&vnode_lock);
    *out           = cache_find(fs, ino);
    spin_unlock_irqrestore(&vnode_lock, flags);
    if (*out) {
        return 0;
    }

    struct vnode *vn = kmalloc(sizeof(*vn));
    if (!vn) {
        return -ENOMEM;
    }
    memset(vn, 0, sizeof(*vn));
    vn->fs  = fs;
    vn->ino = ino;
    int err = fs->ops->vget(fs, ino, vn);
    if (err) {
        kfree(vn);
        return err;
    }

    flags = spin_lock_irqsave(&vnode_lock);
    *out  = cache_find(fs, ino);
    if (!*out) {
        vn->refs = 1;
        vn->next = vnodes;
        vnodes   = vn;
        *out     = vn;
        vn       = nullptr;
    }
    spin_unlock_irqrestore(&vnode_lock, flags);

    if (vn) {
        if (fs->ops->vput) {
            fs->ops->vput(vn);
        }
        kfree(vn);
    }
    return 0;
}

struct vnode *vnode_ref(struct vnode *vn) {
    uint64_t flags = spin_lock_irqsave(&vnode_lock);
    vn->refs++;
    spin_unlock_irqrestore(&vnode_lock, flags);
    return vn;
}

static void vnodes_unlink(struct vnode *vn) {
    for (struct vnode **pp = &vnodes; *pp; pp = &(*pp)->next) {
        if (*pp == vn) {
            *pp = vn->next;
            break;
        }
    }
}

static void release(struct vnode *vn) {
    if (vn->fs->ops->vput) {
        vn->fs->ops->vput(vn);
    }
    kfree(vn);
}

void vnode_put(struct vnode *vn) {
    uint64_t flags = spin_lock_irqsave(&vnode_lock);
    int      last  = --vn->refs == 0;
    if (last) {
        vnodes_unlink(vn);
    }
    spin_unlock_irqrestore(&vnode_lock, flags);

    if (last) {
        release(vn);
    }
}

static int is_dot(const char *name, size_t len) {
    return name[0] == '.' && (len == 1 || (len == 2 && name[1] == '.'));
}

struct vnode *vfs_root() { return vnode_ref(root_mount->root); }

static struct vnode *root_of() { return vnode_ref(root_mount->root); }

static struct vnode *cwd_of() {
    struct proc  *p     = curproc();
    uint64_t      flags = spin_lock_irqsave(&p->files_lock);
    struct vnode *cwd   = vnode_ref(p->cwd ? p->cwd : root_mount->root);
    spin_unlock_irqrestore(&p->files_lock, flags);
    return cwd;
}

static struct vnode *underneath(struct vnode *vn) {
    while (vn->root_of && vn->root_of->covered) {
        vn = vn->root_of->covered;
    }
    return vn;
}

static int at_top(struct vnode *vn) { return vn == root_mount->root; }

static int parent_of(struct vnode *vn, struct vnode **out) {
    vn = underneath(vn);
    if (at_top(vn)) {
        *out = vnode_ref(vn);
        return 0;
    }
    uint64_t ino = 0;
    int      err = vn->fs->vops->lookup(vn, "..", 2, &ino);
    return err ? err : vnode_get(vn->fs, ino, out);
}

static int child_of(struct vnode *dir, const char *name, size_t len,
                    struct vnode **out) {
    if (is_dot(name, len)) {
        if (len == 1) {
            *out = vnode_ref(dir);
            return 0;
        }
        return parent_of(dir, out);
    }
    uint64_t ino;
    int      err = dir->fs->vops->lookup(dir, name, len, &ino);
    if (err) {
        return err;
    }
    struct vnode *vn;
    if ((err = vnode_get(dir->fs, ino, &vn)) != 0) {
        return err;
    }
    while (vn->covered_by) {
        struct vnode *root = vnode_ref(vn->covered_by->root);
        vnode_put(vn);
        vn = root;
    }
    *out = vn;
    return 0;
}

static long read_link(struct vnode *vn, char *buf, size_t n) {
    return vn->fs->vops->readlink ? vn->fs->vops->readlink(vn, buf, n)
                                  : -EINVAL;
}

#define W_PARENT 0x100

struct walk {
    struct vnode *dir;
    struct vnode *vn;
    char          name[NAME_MAX + 1];
    size_t        len;
    int           slash;
};

static int walk(struct vnode *base, const char *path, int flags,
                struct walk *w) {
    size_t plen = strlen(path);
    if (!plen) {
        return -ENOENT;
    }
    if (plen > PATH_MAX) {
        return -ENAMETOOLONG;
    }
    char *buf  = kmalloc(PATH_MAX);
    char *link = kmalloc(PATH_MAX);
    if (!buf || !link) {
        kfree(buf);
        kfree(link);
        return -ENOMEM;
    }
    memcpy(buf, path, plen + 1);

    struct vnode *cur = path[0] == '/' ? root_of()
                        : base         ? vnode_ref(base)
                                       : cwd_of();

    int   links = 0, err = 0;
    char *p = buf;
    *w      = (struct walk){0};
    while (1) {
        while (*p == '/') {
            p++;
        }
        if (!*p) {
            if (flags & W_PARENT) {
                w->dir = cur;
                w->vn  = vnode_ref(cur);
            } else {
                w->vn = cur;
            }
            cur = nullptr;
            break;
        }

        const char *c = p;
        while (*p && *p != '/') {
            p++;
        }
        size_t      n = (size_t)(p - c);
        const char *q = p;
        while (*q == '/') {
            q++;
        }
        int last  = !*q;
        int slash = last && q != p;
        if (n > NAME_MAX) {
            err = -ENAMETOOLONG;
            break;
        }
        if (!S_ISDIR(cur->mode)) {
            err = -ENOTDIR;
            break;
        }
        if (last && (flags & W_PARENT)) {
            memcpy(w->name, c, n);
            w->name[n] = '\0';
            w->len     = n;
            w->slash   = slash;
            w->dir     = cur;
            cur        = nullptr;
            break;
        }
        struct vnode *next;
        if ((err = child_of(cur, c, n, &next)) != 0) {
            break;
        }
        if (S_ISLNK(next->mode) && (!last || (flags & VFS_FOLLOW) || slash)) {
            long tl = ++links > SYMLOOP_MAX ? -ELOOP
                                            : read_link(next, link, PATH_MAX);
            vnode_put(next);
            size_t rest = strlen(p);
            if (tl >= 0 && (size_t)tl + rest >= PATH_MAX) {
                tl = -ENAMETOOLONG;
            }
            if (tl <= 0) {
                err = tl ? (int)tl : -ENOENT;
                break;
            }
            memmove(buf + tl, p, rest + 1);
            memcpy(buf, link, (size_t)tl);
            p = buf;
            if (link[0] == '/') {
                vnode_put(cur);
                cur = root_of();
            }
            continue;
        }
        vnode_put(cur);
        cur = next;
        if (slash && !S_ISDIR(cur->mode)) {
            err = -ENOTDIR;
            break;
        }
    }
    if (cur) {
        vnode_put(cur);
    }
    kfree(buf);
    kfree(link);
    return err;
}

int vfs_lookup(struct vnode *base, const char *path, int flags,
               struct vnode **out) {
    struct walk w;
    int         err = walk(base, path, flags & VFS_FOLLOW, &w);
    if (!err) {
        *out = w.vn;
    }
    return err;
}

static struct mount *mount_new(struct fs *fs) {
    struct mount *m = kmalloc(sizeof(*m));
    if (!m) {
        return nullptr;
    }
    fs->dev    = next_dev++;
    m->fs      = fs;
    m->covered = nullptr;
    m->path[0] = '/';
    m->path[1] = '\0';
    m->next    = nullptr;
    if (vnode_get(fs, fs->root_ino, &m->root) != 0) {
        kfree(m);
        return nullptr;
    }
    m->root->root_of = m;
    return m;
}

int vfs_mount_root(struct fs *fs) {
    struct mount *m = mount_new(fs);
    if (!m) {
        return -ENOMEM;
    }
    root_mount = m;
    return 0;
}

int vfs_mount(const char *path, struct fs *fs) {
    struct vnode *vn;
    int           err = vfs_lookup(nullptr, path, VFS_FOLLOW, &vn);
    if (err) {
        return err;
    }
    if (!S_ISDIR(vn->mode) || vn->covered_by) {
        err = S_ISDIR(vn->mode) ? -EBUSY : -ENOTDIR;
        vnode_put(vn);
        return err;
    }

    struct mount *m = mount_new(fs);
    if (!m) {
        vnode_put(vn);
        return -ENOMEM;
    }
    m->covered     = vn;
    vn->covered_by = m;
    size_t i       = 0;
    for (; path[i] && i < sizeof(m->path) - 1; i++) {
        m->path[i] = path[i];
    }
    m->path[i]         = '\0';
    struct mount **end = &mount_list;
    while (*end) {
        end = &(*end)->next;
    }
    *end = m;
    return 0;
}

#define BOUNCE 4096

long vnode_read_user(struct vnode *vn, void *ubuf, size_t n, uint64_t off) {
    char *b = kmalloc(BOUNCE);
    if (!b) {
        return -ENOMEM;
    }
    size_t done = 0;
    long   err  = 0;
    while (done < n) {
        size_t k   = n - done < BOUNCE ? n - done : BOUNCE;
        long   got = vn->fs->vops->read(vn, b, k, off + done);
        if (got > 0 && copy_to_user((char *)ubuf + done, b, (size_t)got) != 0) {
            got = -EFAULT;
        }
        if (got <= 0) {
            err = got;
            break;
        }
        done += (size_t)got;
        if ((size_t)got < k) {
            break;
        }
    }
    kfree(b);
    return done ? (long)done : err;
}

static long vnode_file_read(struct file *f, void *buf, size_t n) {
    struct vnode *vn = f->vnode;
    if (S_ISDIR(vn->mode)) {
        return -EISDIR;
    }
    long got = vnode_read_user(vn, buf, n, (uint64_t)f->offset);
    if (got > 0) {
        f->offset += got;
    }
    return got;
}

static long vnode_file_size(struct file *f) { return (long)f->vnode->size; }

static const struct file_ops vnode_file_ops = {
    .read = vnode_file_read,
    .size = vnode_file_size,
};

int vfs_open_vnode(struct vnode *vn, int flags, struct file **out) {
    int acc = flags & O_ACCMODE;
    if (S_ISREG(vn->mode) || S_ISDIR(vn->mode)) {
        if (acc != O_RDONLY && S_ISDIR(vn->mode)) {
            return -EISDIR;
        }
        if (acc != O_RDONLY) {
            return -EROFS;
        }
        if (!(*out = file_new(&vnode_file_ops, flags, nullptr))) {
            return -ENOMEM;
        }
        (*out)->vnode = vnode_ref(vn);
        return 0;
    }
    if (S_ISLNK(vn->mode)) {
        return -ELOOP;
    }
    return vn->fs->ops->open ? vn->fs->ops->open(vn, flags, out) : -ENXIO;
}

static int open_path(struct vnode *base, const char *path, int flags,
                     struct file **out) {
    struct vnode *vn;
    int           err =
        vfs_lookup(base, path, (flags & O_NOFOLLOW) ? 0 : VFS_FOLLOW, &vn);
    if (err) {
        return err;
    }
    if ((flags & O_DIRECTORY) && !S_ISDIR(vn->mode)) {
        err = -ENOTDIR;
    } else if (!(*out = file_new(&vnode_file_ops, O_PATH, nullptr))) {
        err = -ENOMEM;
    } else {
        (*out)->vnode = vn;
        return 0;
    }
    vnode_put(vn);
    return err;
}

int vfs_open(struct vnode *base, const char *path, int flags, uint32_t mode,
             struct file **out) {
    if (flags & O_PATH) {
        return open_path(base, path, flags, out);
    }
    if ((flags & O_CREAT) && (flags & O_DIRECTORY)) {
        return -EINVAL;
    }
    struct vnode *vn;
    (void)mode;
    int err =
        vfs_lookup(base, path, (flags & O_NOFOLLOW) ? 0 : VFS_FOLLOW, &vn);
    if (err) {
        return err == -ENOENT && (flags & O_CREAT) ? -EROFS : err;
    }

    if ((flags & O_NOFOLLOW) && S_ISLNK(vn->mode)) {
        err = -ELOOP;
    } else if ((flags & O_DIRECTORY) && !S_ISDIR(vn->mode)) {
        err = -ENOTDIR;
    }
    if (!err) {
        err = vfs_open_vnode(vn, flags, out);
    }
    vnode_put(vn);
    return err;
}

long vnode_read(struct vnode *vn, void *buf, size_t n, uint64_t off) {
    size_t got = 0;
    while (got < n) {
        long r =
            vn->fs->vops->read(vn, (uint8_t *)buf + got, n - got, off + got);
        if (r <= 0) {
            if (r < 0) {
                return r;
            }
            break;
        }
        got += (size_t)r;
    }
    return (long)got;
}

int vfs_exec_check(struct vnode *vn) { return S_ISREG(vn->mode) ? 0 : -EACCES; }

int vfs_exec_open(const char *path, struct vnode **out) {
    struct vnode *vn;
    int           err = vfs_lookup(nullptr, path, VFS_FOLLOW, &vn);
    if (err) {
        return err;
    }
    err = S_ISREG(vn->mode) ? 0 : -EACCES;
    if (err) {
        vnode_put(vn);
        return err;
    }
    *out = vn;
    return 0;
}

int vfs_chdir(struct vnode *dir) {
    if (!S_ISDIR(dir->mode)) {
        return -ENOTDIR;
    }
    struct proc  *p     = curproc();
    struct vnode *fresh = vnode_ref(dir);
    uint64_t      flags = spin_lock_irqsave(&p->files_lock);
    struct vnode *old   = p->cwd;
    p->cwd              = fresh;
    spin_unlock_irqrestore(&p->files_lock, flags);
    if (old) {
        vnode_put(old);
    }
    return 0;
}

static int name_in(struct vnode *parent, struct vnode *vn,
                   struct vfs_dirent *de) {
    uint64_t pos = 0;
    int      got;
    while ((got = parent->fs->vops->readdir(parent, &pos, de)) > 0) {
        size_t n = strlen(de->name);
        if (de->ino == vn->ino && !is_dot(de->name, n)) {
            return 0;
        }
    }
    return got < 0 ? got : -ENOENT;
}

long vfs_getcwd(char *buf, size_t size) {
    char              *path = kmalloc(PATH_MAX);
    struct vfs_dirent *de   = kmalloc(sizeof(*de));
    struct vnode      *vn   = cwd_of();
    size_t             at   = PATH_MAX - 1;
    long               err  = !path || !de ? -ENOMEM : 0;
    if (!err && vn->nlink == 0) {
        err = -ENOENT;
    }
    if (!err) {
        path[at] = '\0';
    }
    while (!err) {
        struct vnode *here = underneath(vn), *up;
        if (at_top(here)) {
            break;
        }
        if ((err = parent_of(here, &up)) != 0) {
            break;
        }
        if ((err = name_in(up, here, de)) == 0) {
            size_t n = strlen(de->name);
            if (n + 1 > at) {
                err = -ENAMETOOLONG;
            } else {
                at -= n;
                memcpy(path + at, de->name, n);
                path[--at] = '/';
            }
        }
        vnode_put(vn);
        vn = up;
    }
    vnode_put(vn);
    if (!err && at == PATH_MAX - 1) {
        path[--at] = '/';
    }
    size_t len = PATH_MAX - at;
    if (!err && size < len) {
        err = -ERANGE;
    }
    if (!err) {
        memcpy(buf, path + at, len);
        err = (long)len;
    }
    kfree(path);
    kfree(de);
    return err;
}

long vfs_lseek(struct file *f, long offset, int whence) {
    if (!f->ops->size) {
        return -ESPIPE;
    }

    long base;
    switch (whence) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = f->offset;
        break;
    case SEEK_END:
        base = f->ops->size(f);
        break;
    default:
        return -EINVAL;
    }
    if (base + offset < 0) {
        return -EINVAL;
    }
    f->offset = base + offset;
    return f->offset;
}

void vfs_stat(struct vnode *vn, struct stat *st) {
    memset(st, 0, sizeof(*st));
    st->st_dev     = vn->fs->dev;
    st->st_ino     = vn->ino;
    st->st_rdev    = vn->rdev;
    st->st_mode    = vn->mode;
    st->st_nlink   = vn->nlink;
    st->st_uid     = vn->uid;
    st->st_gid     = vn->gid;
    st->st_size    = (int64_t)vn->size;
    st->st_blksize = 4096;
    st->st_blocks  = (int64_t)((vn->size + 511) / 512);
    st->st_atime   = vn->atime;
    st->st_mtime   = vn->mtime;
    st->st_ctime   = vn->ctime;
}
