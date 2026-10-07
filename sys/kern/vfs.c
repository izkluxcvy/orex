#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <kmalloc.h>
#include <memcpy.h>
#include <memmove.h>
#include <memset.h>
#include <spinlock.h>
#include <vfs.h>

struct mount {
    struct fs    *fs;
    struct vnode *root;
    struct vnode *covered;
    char          path[64];
    struct mount *next;
};

static struct mount *mount_list;

static struct spinlock vnode_lock = {.name = "vnode"};

static struct vnode *vnodes;
static struct mount *root_mount;
static uint32_t      next_dev = 1;

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

static struct vnode *cwd_of() { return vnode_ref(root_mount->root); }

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
