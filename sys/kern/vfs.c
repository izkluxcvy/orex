#include <stddef.h>
#include <stdint.h>

#include <blk.h>
#include <cred.h>
#include <errno.h>
#include <kmalloc.h>
#include <ktime.h>
#include <memcpy.h>
#include <memmove.h>
#include <memset.h>
#include <mutex.h>
#include <proc.h>
#include <spinlock.h>
#include <uaccess.h>
#include <vfs.h>
#include <vobj.h>

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
static struct mutex  rename_lock;

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
    if (vn->vobj) {
        vobj_put(vn->vobj);
    }
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

struct vobj *vfs_vobj(struct vnode *vn) {
    if (vn->fs->vops->vobj) {
        return vn->fs->vops->vobj(vn);
    }
    uint64_t flags = spin_lock_irqsave(&vnode_lock);
    if (vn->vobj) {
        struct vobj *o = vobj_ref(vn->vobj);
        spin_unlock_irqrestore(&vnode_lock, flags);
        return o;
    }
    spin_unlock_irqrestore(&vnode_lock, flags);
    struct vobj *fresh = vobj_new(vn);
    if (!fresh) {
        return nullptr;
    }
    flags = spin_lock_irqsave(&vnode_lock);
    if (!vn->vobj) {
        vn->vobj = fresh;
        fresh    = nullptr;
    }
    struct vobj *o = vobj_ref(vn->vobj);
    spin_unlock_irqrestore(&vnode_lock, flags);
    if (fresh) {
        vobj_put(fresh);
    }
    return o;
}

static struct vobj *cache_of(struct vnode *vn) {
    return vn->vobj && vn->vobj->backing ? vn->vobj : nullptr;
}

int vfs_fsync(struct vnode *vn) {
    struct vobj *c   = cache_of(vn);
    int          err = c ? vobj_sync(c) : 0;
    int          b   = blk_sync();
    return err ? err : b;
}

// --- Permissions -------------------------------------------------------------

int vfs_permission(struct vnode *vn, int mask, int real) {
    struct cred *c   = &curproc()->cred;
    uid_t        uid = real ? c->ruid : c->euid;

    if (uid == 0) {
        if ((mask & X_OK) && !S_ISDIR(vn->mode) && !(vn->mode & 0111)) {
            return -EACCES;
        }
        return 0;
    }

    uint32_t bits = vn->mode;
    if (uid == vn->uid) {
        bits >>= 6;
    } else if (cred_in_group(c, vn->gid, real)) {
        bits >>= 3;
    }
    return ((bits & (uint32_t)mask) == (uint32_t)mask) ? 0 : -EACCES;
}

static int sticky_ok(struct vnode *dir, struct vnode *vn) {
    uid_t euid = curproc()->cred.euid;
    return !(dir->mode & S_ISVTX) || euid == 0 || euid == dir->uid ||
           euid == vn->uid;
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
        if ((err = vfs_permission(cur, X_OK, flags & VFS_REAL)) != 0) {
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

static void walk_done(struct walk *w) {
    if (w->dir) {
        vnode_put(w->dir);
    }
    if (w->vn) {
        vnode_put(w->vn);
    }
}

int vfs_lookup(struct vnode *base, const char *path, int flags,
               struct vnode **out) {
    struct walk w;
    int         err = walk(base, path, flags & (VFS_FOLLOW | VFS_REAL), &w);
    if (!err) {
        *out = w.vn;
    }
    return err;
}

static int walk_parent(struct vnode *base, const char *path, struct walk *w) {
    int err = walk(base, path, W_PARENT, w);
    if (!err && w->len) {
        int e = child_of(w->dir, w->name, w->len, &w->vn);
        if (e && e != -ENOENT) {
            walk_done(w);
            return e;
        }
    }
    return err;
}

static int may_create(struct vnode *dir) {
    if (dir->nlink == 0) {
        return -ENOENT;
    }
    return vfs_permission(dir, W_OK | X_OK, 0);
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
    mutex_init(&rename_lock);
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

long vnode_write(struct vnode *vn, const void *buf, size_t n, uint64_t off) {
    if (n) {
        vfs_touch(vn, VFS_MTIME | VFS_CTIME, 0);
    }
    return vn->fs->vops->write(vn, buf, n, off);
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
        if (got > 0 && cache_of(vn)) {
            vobj_overlay(vn->vobj, off + done, b, (size_t)got);
        }
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

long vnode_write_user(struct vnode *vn, const void *ubuf, size_t n,
                      uint64_t off) {
    char *b = kmalloc(BOUNCE);
    if (!b) {
        return -ENOMEM;
    }
    size_t done = 0;
    long   err  = 0;
    while (done < n) {
        size_t k = n - done < BOUNCE ? n - done : BOUNCE;
        if (copy_from_user(b, (const char *)ubuf + done, k) != 0) {
            err = -EFAULT;
            break;
        }
        long put = vnode_write(vn, b, k, off + done);
        if (put > 0 && cache_of(vn)) {
            vobj_update(vn->vobj, off + done, b, (size_t)put);
        }
        if (put <= 0) {
            err = put;
            break;
        }
        done += (size_t)put;
        if ((size_t)put < k) {
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
        vfs_accessed(vn);
    }
    return got;
}

static struct mutex append_lock;

static long vnode_file_write(struct file *f, const void *buf, size_t n) {
    struct vnode *vn     = f->vnode;
    int           append = f->flags & O_APPEND;
    if (append) {
        mutex_lock(&append_lock);
        f->offset = (long)vn->size;
    }
    long done = vnode_write_user(vn, buf, n, (uint64_t)f->offset);
    if (done > 0) {
        f->offset += done;
    }
    if (append) {
        mutex_unlock(&append_lock);
    }
    return done;
}

static long vnode_file_size(struct file *f) { return (long)f->vnode->size; }

static const struct file_ops vnode_file_ops = {
    .read  = vnode_file_read,
    .write = vnode_file_write,
    .size  = vnode_file_size,
};

static void new_owner(struct vnode *dir, uint32_t *uid, uint32_t *gid) {
    struct cred *c = &curproc()->cred;
    *uid           = c->euid;
    *gid           = (dir->mode & S_ISGID) ? dir->gid : c->egid;
}

static int make_node(struct vnode *dir, const char *name, size_t len,
                     uint32_t mode, uint64_t rdev, struct vnode **out) {
    uint32_t uid, gid;
    uint64_t ino;
    new_owner(dir, &uid, &gid);
    int err = may_create(dir);
    if (!err) {
        err = dir->fs->vops->create
                  ? dir->fs->vops->create(dir, name, len, mode, rdev, uid, gid,
                                          &ino)
                  : -EROFS;
    }
    if (!err) {
        vfs_touch(dir, VFS_MTIME | VFS_CTIME, 1);
        err = out ? vnode_get(dir->fs, ino, out) : 0;
    }
    return err;
}

int vfs_open_vnode(struct vnode *vn, int flags, struct file **out) {
    int acc = flags & O_ACCMODE;
    int err = 0;
    if (S_ISREG(vn->mode) || S_ISDIR(vn->mode)) {
        if (acc != O_RDONLY && S_ISDIR(vn->mode)) {
            return -EISDIR;
        }
        if (acc != O_RDONLY && !vn->fs->vops->write) {
            return -EROFS;
        }
        if ((flags & O_TRUNC) && acc != O_RDONLY && S_ISREG(vn->mode) &&
            (err = vfs_truncate(vn, 0)) != 0) {
            return err;
        }
        if (!(*out = file_new(&vnode_file_ops, flags, nullptr))) {
            return -ENOMEM;
        }
        (*out)->vnode = vnode_ref(vn);
        return 0;
    }
    if (S_ISFIFO(vn->mode)) {
        return fifo_open(vn, flags, out);
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

static int open_creat(struct vnode *base, const char *path, int flags,
                      uint32_t mode, struct vnode **out, int *created,
                      int depth) {
    struct walk w;
    int         err = walk_parent(base, path, &w);
    if (err) {
        return err;
    }
    if (!w.len || is_dot(w.name, w.len) || w.slash) {
        err = -EISDIR;
    } else if (!w.vn) {
        err = make_node(w.dir, w.name, w.len,
                        S_IFREG | (mode & 0777 & ~curproc()->umask), 0, out);
        *created = !err;
        if (err == -EEXIST && !(flags & O_EXCL)) {
            walk_done(&w);
            return open_creat(base, path, flags, mode, out, created, depth);
        }
    } else if (flags & O_EXCL) {
        err = -EEXIST;
    } else if (S_ISLNK(w.vn->mode) && (flags & O_NOFOLLOW)) {
        err = -ELOOP;
    } else if (S_ISLNK(w.vn->mode)) {
        char *target = kmalloc(PATH_MAX);
        long  n = !target                ? -ENOMEM
                  : depth >= SYMLOOP_MAX ? -ELOOP
                                         : read_link(w.vn, target, PATH_MAX);
        if (n > 0) {
            target[n] = '\0';
            err =
                open_creat(base, target, flags, mode, out, created, depth + 1);
        } else {
            err = n ? (int)n : -ENOENT;
        }
        kfree(target);
    } else {
        *out = vnode_ref(w.vn);
    }
    walk_done(&w);
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
    int           created = 0, err;
    if (flags & O_CREAT) {
        err = open_creat(base, path, flags, mode, &vn, &created, 0);
    } else {
        err =
            vfs_lookup(base, path, (flags & O_NOFOLLOW) ? 0 : VFS_FOLLOW, &vn);
    }
    if (err) {
        return err;
    }

    int acc  = flags & O_ACCMODE;
    int need = (acc == O_RDONLY   ? R_OK
                : acc == O_WRONLY ? W_OK
                                  : R_OK | W_OK) |
               ((flags & O_TRUNC) ? W_OK : 0);
    if ((flags & O_NOFOLLOW) && S_ISLNK(vn->mode)) {
        err = -ELOOP;
    } else if ((flags & O_DIRECTORY) && !S_ISDIR(vn->mode)) {
        err = -ENOTDIR;
    } else if (!created) {
        err = vfs_permission(vn, need, 0);
    }
    if (!err) {
        err = vfs_open_vnode(vn, flags & ~(created ? O_TRUNC : 0), out);
    }
    vnode_put(vn);
    return err;
}

int vfs_mkdir(struct vnode *base, const char *path, uint32_t mode) {
    struct walk w;
    int         err = walk_parent(base, path, &w);
    if (err) {
        return err;
    }
    if (!w.len || w.vn) {
        err = -EEXIST;
    } else if ((err = may_create(w.dir)) == 0) {
        uint32_t uid, gid;
        new_owner(w.dir, &uid, &gid);
        mode = (mode & 0777 & ~curproc()->umask) | (w.dir->mode & S_ISGID);
        err = w.dir->fs->vops->mkdir
                  ? w.dir->fs->vops->mkdir(w.dir, w.name, w.len, S_IFDIR | mode,
                                           uid, gid)
                  : -EROFS;
        if (!err) {
            vfs_touch(w.dir, VFS_MTIME | VFS_CTIME, 1);
        }
    }
    walk_done(&w);
    return err;
}

int vfs_mknod(struct vnode *base, const char *path, uint32_t mode,
              uint64_t rdev) {
    uint32_t type = mode & S_IFMT;
    if (!type) {
        type = S_IFREG;
    }
    if (type != S_IFREG && type != S_IFIFO && type != S_IFCHR &&
        type != S_IFBLK && type != S_IFSOCK) {
        return -EINVAL;
    }
    struct walk w;
    int         err = walk_parent(base, path, &w);
    if (err) {
        return err;
    }
    if (!w.len || w.vn) {
        err = -EEXIST;
    } else if (w.slash) {
        err = -ENOENT;
    } else {
        err = make_node(w.dir, w.name, w.len,
                        type | (mode & 0777 & ~curproc()->umask),
                        type == S_IFCHR || type == S_IFBLK ? rdev : 0, nullptr);
    }
    walk_done(&w);
    return err;
}

int vfs_symlink(const char *target, struct vnode *base, const char *path) {
    size_t tlen = strlen(target);
    if (!tlen) {
        return -ENOENT;
    }
    if (tlen >= PATH_MAX) {
        return -ENAMETOOLONG;
    }
    struct walk w;
    int         err = walk_parent(base, path, &w);
    if (err) {
        return err;
    }
    if (!w.len || w.vn) {
        err = -EEXIST;
    } else if (w.slash) {
        err = -ENOENT;
    } else if ((err = may_create(w.dir)) == 0) {
        uint32_t uid, gid;
        new_owner(w.dir, &uid, &gid);
        err = w.dir->fs->vops->symlink
                  ? w.dir->fs->vops->symlink(w.dir, w.name, w.len, target, tlen,
                                             uid, gid)
                  : -EROFS;
        if (!err) {
            vfs_touch(w.dir, VFS_MTIME | VFS_CTIME, 1);
        }
    }
    walk_done(&w);
    return err;
}

long vfs_readlink(struct vnode *base, const char *path, char *buf, size_t n) {
    struct vnode *vn;
    long          err = vfs_lookup(base, path, 0, &vn);
    if (err) {
        return err;
    }
    err = S_ISLNK(vn->mode) ? read_link(vn, buf, n) : -EINVAL;
    if (err >= 0) {
        vfs_accessed(vn);
    }
    vnode_put(vn);
    return err;
}

#define LINK_MAX 65000

int vfs_link(struct vnode *obase, const char *opath, struct vnode *nbase,
             const char *npath, int flags) {
    struct vnode *vn;
    int           err = vfs_lookup(obase, opath, flags & VFS_FOLLOW, &vn);
    if (err) {
        return err;
    }
    struct walk w;
    if ((err = walk_parent(nbase, npath, &w)) != 0) {
        vnode_put(vn);
        return err;
    }
    if (S_ISDIR(vn->mode)) {
        err = -EPERM;
    } else if (!w.len || w.vn) {
        err = -EEXIST;
    } else if (w.slash) {
        err = -ENOENT;
    } else if (w.dir->fs != vn->fs) {
        err = -EXDEV;
    } else if (vn->nlink >= LINK_MAX) {
        err = -EMLINK;
    } else if ((err = may_create(w.dir)) == 0) {
        vfs_touch(vn, VFS_CTIME, 0);
        err = w.dir->fs->vops->link
                  ? w.dir->fs->vops->link(w.dir, w.name, w.len, vn)
                  : -EROFS;
        if (!err) {
            vfs_touch(w.dir, VFS_MTIME | VFS_CTIME, 1);
        }
    }
    walk_done(&w);
    vnode_put(vn);
    return err;
}

static int remove(struct vnode *base, const char *path, int want_dir) {
    struct walk w;
    int         err = walk_parent(base, path, &w);
    if (err) {
        return err;
    }
    struct vnode *dir = w.dir, *victim = w.vn;
    if (!w.len) {
        err = -EBUSY;
    } else if (is_dot(w.name, w.len)) {
        err = !want_dir ? -EISDIR : w.len == 1 ? -EINVAL : -ENOTEMPTY;
    } else if (!victim) {
        err = -ENOENT;
    } else if ((err = vfs_permission(dir, W_OK | X_OK, 0)) != 0) {
    } else if (victim->covered_by || victim->root_of) {
        err = -EBUSY;
    } else if (!sticky_ok(dir, victim)) {
        err = -EPERM;
    } else if (want_dir && !S_ISDIR(victim->mode)) {
        err = -ENOTDIR;
    } else if (!want_dir && S_ISDIR(victim->mode)) {
        err = -EISDIR;
    } else if (!want_dir && w.slash) {
        err = -ENOTDIR;
    } else {
        int (*op)(struct vnode *, const char *, size_t, struct vnode *) =
            want_dir ? dir->fs->vops->rmdir : dir->fs->vops->unlink;
        vfs_touch(victim, VFS_CTIME, 0);
        err = op ? op(dir, w.name, w.len, victim) : -EROFS;
        if (!err) {
            vfs_touch(dir, VFS_MTIME | VFS_CTIME, 1);
        }
    }
    walk_done(&w);
    return err;
}

int vfs_unlink(struct vnode *base, const char *path) {
    return remove(base, path, 0);
}
int vfs_rmdir(struct vnode *base, const char *path) {
    return remove(base, path, 1);
}

static int is_below(struct vnode *dir, struct vnode *vn) {
    struct vnode *cur   = vnode_ref(dir);
    int           below = 0;
    while (!below) {
        if (cur == vn) {
            below = 1;
            break;
        }
        if (cur->root_of || cur == root_mount->root) {
            break;
        }
        struct vnode *up;
        if (parent_of(cur, &up) != 0) {
            break;
        }
        vnode_put(cur);
        cur = up;
    }
    vnode_put(cur);
    return below;
}

static int rename_checks(struct walk *o, struct walk *n, int flags) {
    struct vnode *moved = o->vn, *victim = n->vn;
    int           err;
    if (!o->len || !n->len) {
        return -EBUSY;
    }
    if (is_dot(o->name, o->len) || is_dot(n->name, n->len)) {
        return -EINVAL;
    }
    if (!moved) {
        return -ENOENT;
    }
    if (o->dir->fs != n->dir->fs) {
        return -EXDEV;
    }
    if ((err = vfs_permission(o->dir, W_OK | X_OK, 0)) != 0 ||
        (err = may_create(n->dir)) != 0) {
        return err;
    }
    if (!sticky_ok(o->dir, moved) || (victim && !sticky_ok(n->dir, victim))) {
        return -EPERM;
    }
    if (moved->covered_by || moved->root_of ||
        (victim && (victim->covered_by || victim->root_of))) {
        return -EBUSY;
    }
    int is_dir = S_ISDIR(moved->mode);
    if ((o->slash || n->slash) && !is_dir) {
        return -ENOTDIR;
    }
    if (victim && victim != moved) {
        if (flags & RENAME_NOREPLACE) {
            return -EEXIST;
        }
        if (is_dir && !S_ISDIR(victim->mode)) {
            return -ENOTDIR;
        }
        if (!is_dir && S_ISDIR(victim->mode)) {
            return -EISDIR;
        }
    }
    if (is_dir && o->dir != n->dir) {
        if (is_below(n->dir, moved)) {
            return -EINVAL;
        }
        if ((err = vfs_permission(moved, W_OK, 0)) != 0) {
            return err;
        }
    }
    return 0;
}

int vfs_rename(struct vnode *obase, const char *opath, struct vnode *nbase,
               const char *npath, int flags) {
    if (flags & ~RENAME_NOREPLACE) {
        return -EINVAL;
    }
    struct walk o, n;
    int         err = walk_parent(obase, opath, &o);
    if (err) {
        return err;
    }
    if ((err = walk_parent(nbase, npath, &n)) != 0) {
        walk_done(&o);
        return err;
    }
    mutex_lock(&rename_lock);
    err = rename_checks(&o, &n, flags);
    if (!err && n.vn != o.vn) {
        struct vnode *dir = n.dir;
        vfs_touch(o.vn, VFS_CTIME, 0);
        if (n.vn) {
            vfs_touch(n.vn, VFS_CTIME, 0);
        }
        err = dir->fs->vops->rename
                  ? dir->fs->vops->rename(o.dir, o.name, o.len, dir, n.name,
                                          n.len, o.vn, n.vn)
                  : -EROFS;
        if (!err) {
            vfs_touch(o.dir, VFS_MTIME | VFS_CTIME, 1);
            vfs_touch(dir, VFS_MTIME | VFS_CTIME, 1);
        }
    }
    mutex_unlock(&rename_lock);
    walk_done(&n);
    walk_done(&o);
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
    if (got && cache_of(vn)) {
        vobj_overlay(vn->vobj, off, buf, got);
    }
    return (long)got;
}

int vfs_exec_check(struct vnode *vn) {
    return S_ISREG(vn->mode) ? vfs_permission(vn, X_OK, 0) : -EACCES;
}

int vfs_exec_open(const char *path, struct vnode **out) {
    struct vnode *vn;
    int           err = vfs_lookup(nullptr, path, VFS_FOLLOW, &vn);
    if (err) {
        return err;
    }
    err = S_ISREG(vn->mode) ? vfs_permission(vn, X_OK, 0) : -EACCES;
    if (err) {
        vnode_put(vn);
        return err;
    }
    *out = vn;
    return 0;
}

int vfs_access(struct vnode *base, const char *path, int mask, int flags) {
    struct vnode *vn;
    int           err = vfs_lookup(base, path, flags, &vn);
    if (err) {
        return err;
    }
    int real = (flags & VFS_REAL) != 0;
    err      = mask ? vfs_permission(vn, mask, real) : 0;
    if (!err && (mask & W_OK) &&
        (S_ISREG(vn->mode) || S_ISDIR(vn->mode) || S_ISLNK(vn->mode)) &&
        !vn->fs->vops->write) {
        err = -EROFS;
    }
    vnode_put(vn);
    return err;
}

int vfs_truncate(struct vnode *vn, int64_t size) {
    if (S_ISDIR(vn->mode)) {
        return -EISDIR;
    }
    if (!S_ISREG(vn->mode) || size < 0) {
        return -EINVAL;
    }
    if (!vn->fs->vops->truncate) {
        return -EROFS;
    }
    vfs_touch(vn, VFS_MTIME | VFS_CTIME, 0);
    int err = vn->fs->vops->truncate(vn, (uint64_t)size);
    if (!err && cache_of(vn)) {
        vobj_truncate(vn->vobj, (uint64_t)size);
    }
    return err;
}

int vfs_chdir(struct vnode *dir) {
    if (!S_ISDIR(dir->mode)) {
        return -ENOTDIR;
    }
    int err = vfs_permission(dir, X_OK, 0);
    if (err) {
        return err;
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

void vfs_touch(struct vnode *vn, int what, int sync) {
    uint64_t now = (uint64_t)(time_real() / NSEC);
    if (what & VFS_ATIME) {
        vn->atime = now;
    }
    if (what & VFS_MTIME) {
        vn->mtime = now;
    }
    if (what & VFS_CTIME) {
        vn->ctime = now;
    }
    if (sync && vn->fs->vops->setattr) {
        vn->fs->vops->setattr(vn);
    }
}

void vfs_accessed(struct vnode *vn) {
    uint64_t now = (uint64_t)(time_real() / NSEC);
    if (vn->fs->vops->setattr &&
        (vn->atime <= vn->mtime || vn->atime <= vn->ctime ||
         now - vn->atime >= 24 * 3600)) {
        vfs_touch(vn, VFS_ATIME, 1);
    }
}

#define UTIME_NOW  ((1L << 30) - 1)
#define UTIME_OMIT ((1L << 30) - 2)

int vfs_utimes(struct vnode *vn, const struct k_timespec *ts) {
    for (int i = 0; ts && i < 2; i++) {
        if (ts[i].nsec != UTIME_NOW && ts[i].nsec != UTIME_OMIT &&
            (ts[i].nsec < 0 || ts[i].nsec >= NSEC)) {
            return -EINVAL;
        }
    }
    int only_now = !ts || (ts[0].nsec == UTIME_NOW && ts[1].nsec == UTIME_NOW);
    if (ts && ts[0].nsec == UTIME_OMIT && ts[1].nsec == UTIME_OMIT) {
        return 0;
    }
    struct cred *c = &curproc()->cred;
    if (c->euid != 0 && c->euid != vn->uid) {
        if (!only_now) {
            return -EPERM;
        }
        int err = vfs_permission(vn, W_OK, 0);
        if (err) {
            return err;
        }
    }
    if (!vn->fs->vops->setattr) {
        return -EROFS;
    }
    uint64_t  now      = (uint64_t)(time_real() / NSEC);
    uint64_t *field[2] = {&vn->atime, &vn->mtime};
    for (int i = 0; i < 2; i++) {
        if (!ts || ts[i].nsec == UTIME_NOW) {
            *field[i] = now;
        } else if (ts[i].nsec != UTIME_OMIT) {
            *field[i] = (uint64_t)ts[i].sec;
        }
    }
    vn->ctime = now;
    return vn->fs->vops->setattr(vn);
}

int vfs_chmod(struct vnode *vn, uint32_t mode) {
    struct cred *c = &curproc()->cred;
    if (c->euid != 0 && c->euid != vn->uid) {
        return -EPERM;
    }
    if (!vn->fs->vops->setattr) {
        return -EROFS;
    }
    mode &= 07777;
    vfs_touch(vn, VFS_CTIME, 0);
    if (c->euid != 0 && !cred_in_group(c, vn->gid, 0)) {
        mode &= ~(uint32_t)S_ISGID;
    }
    vn->mode = (vn->mode & S_IFMT) | mode;
    return vn->fs->vops->setattr(vn);
}

int vfs_chown(struct vnode *vn, uint32_t uid, uint32_t gid) {
    struct cred *c = &curproc()->cred;
    if (uid == (uint32_t)-1) {
        uid = vn->uid;
    }
    if (gid == (uint32_t)-1) {
        gid = vn->gid;
    }
    if (c->euid != 0 &&
        (c->euid != vn->uid || (gid != vn->gid && !cred_in_group(c, gid, 0)))) {
        return -EPERM;
    }
    if (!vn->fs->vops->setattr) {
        return -EROFS;
    }
    if ((uid != vn->uid || gid != vn->gid) && !S_ISDIR(vn->mode)) {
        vn->mode &= ~(uint32_t)(S_ISUID | S_ISGID);
    }
    vn->uid = uid;
    vn->gid = gid;
    vfs_touch(vn, VFS_CTIME, 0);
    return vn->fs->vops->setattr(vn);
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
