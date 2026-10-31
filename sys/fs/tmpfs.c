#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <kmalloc.h>
#include <ktime.h>
#include <memcmp.h>
#include <memcpy.h>
#include <memset.h>
#include <mutex.h>
#include <tmpfs.h>
#include <vfs.h>
#include <vobj.h>

struct tdirent;

struct tnode {
    uint64_t        ino;
    uint32_t        mode, uid, gid, nlink;
    uint64_t        size, rdev, atime, mtime, ctime;
    struct vobj    *data;
    char           *target;
    struct tdirent *entries;
    struct tnode   *parent;
    int             vrefs;
    struct tnode   *next;
};

struct tdirent {
    char           *name;
    size_t          len;
    struct tnode   *node;
    struct tdirent *next;
};

struct tmpfs {
    struct fs     fs;
    struct mutex  lock;
    struct tnode *nodes;
    uint64_t      next_ino;
};

static struct tmpfs *tfs_of(struct vnode *vn) { return (struct tmpfs *)vn->fs; }
static struct tnode *node_of(struct vnode *vn) { return vn->priv; }

static uint64_t now() { return (uint64_t)(time_real() / NSEC); }

static struct tnode *node_new(struct tmpfs *t, uint32_t mode, uint32_t uid,
                              uint32_t gid) {
    struct tnode *n = kmalloc(sizeof(*n));
    if (!n) {
        return nullptr;
    }
    uint64_t ts = now();
    *n          = (struct tnode){.ino   = t->next_ino++,
                                 .mode  = mode,
                                 .uid   = uid,
                                 .gid   = gid,
                                 .nlink = S_ISDIR(mode) ? 2 : 1,
                                 .atime = ts,
                                 .mtime = ts,
                                 .ctime = ts,
                                 .next  = t->nodes};
    t->nodes    = n;
    return n;
}

static void node_maybe_free(struct tmpfs *t, struct tnode *n) {
    if (n->nlink || n->vrefs) {
        return;
    }
    for (struct tnode **pp = &t->nodes; *pp; pp = &(*pp)->next) {
        if (*pp == n) {
            *pp = n->next;
            break;
        }
    }
    if (n->data) {
        vobj_put(n->data);
    }
    kfree(n->target);
    kfree(n);
}

static struct tdirent *find(struct tnode *dir, const char *name, size_t len) {
    for (struct tdirent *d = dir->entries; d; d = d->next) {
        if (d->len == len && memcmp(d->name, name, len) == 0) {
            return d;
        }
    }
    return nullptr;
}

static int add(struct tnode *dir, const char *name, size_t len,
               struct tnode *n) {
    struct tdirent *d = kmalloc(sizeof(*d));
    char           *s = kmalloc(len + 1);
    if (!d || !s) {
        kfree(d);
        kfree(s);
        return -ENOMEM;
    }
    memcpy(s, name, len);
    s[len]       = '\0';
    *d           = (struct tdirent){s, len, n, dir->entries};
    dir->entries = d;
    return 0;
}

static struct tnode *take(struct tnode *dir, const char *name, size_t len) {
    for (struct tdirent **pp = &dir->entries; *pp; pp = &(*pp)->next) {
        struct tdirent *d = *pp;
        if (d->len == len && memcmp(d->name, name, len) == 0) {
            struct tnode *n = d->node;
            *pp             = d->next;
            kfree(d->name);
            kfree(d);
            return n;
        }
    }
    return nullptr;
}

static void store(struct vnode *vn) {
    struct tnode *n = node_of(vn);
    n->mode         = vn->mode;
    n->uid          = vn->uid;
    n->gid          = vn->gid;
    n->nlink        = vn->nlink;
    n->size         = vn->size;
    n->atime        = vn->atime;
    n->mtime        = vn->mtime;
    n->ctime        = vn->ctime;
}

// --- vnode ops ---------------------------------------------------------------

static int t_lookup(struct vnode *dir, const char *name, size_t len,
                    uint64_t *ino) {
    struct tmpfs *t = tfs_of(dir);
    mutex_lock(&t->lock);
    struct tnode *n = node_of(dir);
    int           err;
    if (len == 2 && name[0] == '.' && name[1] == '.') {
        *ino = n->parent->ino;
        err  = 0;
    } else {
        struct tdirent *d = find(n, name, len);
        err               = d ? 0 : -ENOENT;
        if (d) {
            *ino = d->node->ino;
        }
    }
    mutex_unlock(&t->lock);
    return err;
}

static long t_read(struct vnode *vn, void *buf, size_t n, uint64_t off) {
    struct tnode *node = node_of(vn);
    if (off >= vn->size) {
        return 0;
    }
    if (n > vn->size - off) {
        n = vn->size - off;
    }
    if (node->data) {
        vobj_read(node->data, off, buf, n);
    } else {
        memset(buf, 0, n);
    }
    return (long)n;
}

static long t_write(struct vnode *vn, const void *buf, size_t n, uint64_t off) {
    struct tnode *node = node_of(vn);
    struct tmpfs *t    = tfs_of(vn);
    if (off > TMPFS_MAX || n > TMPFS_MAX - off) {
        return -EFBIG;
    }

    mutex_lock(&t->lock);
    if (!node->data) {
        node->data = vobj_new(nullptr);
    }
    mutex_unlock(&t->lock);

    if (!node->data) {
        return -ENOSPC;
    }
    int err = vobj_write(node->data, off, buf, n);
    if (err) {
        return err;
    }

    mutex_lock(&t->lock);
    if (off + n > vn->size) {
        vn->size = off + n;
    }
    mutex_unlock(&t->lock);

    return (long)n;
}

static int t_truncate(struct vnode *vn, uint64_t size) {
    struct tnode *node = node_of(vn);
    struct tmpfs *t    = tfs_of(vn);
    if (size > TMPFS_MAX) {
        return -EFBIG;
    }

    mutex_lock(&t->lock);
    if (node->data) {
        vobj_truncate(node->data, size);
    }
    vn->size = size;
    mutex_unlock(&t->lock);

    return 0;
}

static size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

static long t_readlink(struct vnode *vn, char *buf, size_t n) {
    struct tnode *node = node_of(vn);
    size_t        len  = strlen(node->target);
    if (n > len) {
        n = len;
    }
    memcpy(buf, node->target, n);
    return (long)n;
}

static int t_readdir(struct vnode *dir, uint64_t *pos, struct vfs_dirent *out) {
    struct tmpfs *t = tfs_of(dir);
    struct tnode *n = node_of(dir);
    if (*pos < 2) {
        out->ino  = *pos == 0 ? n->ino : n->parent->ino;
        out->type = DT_DIR;
        memcpy(out->name, *pos == 0 ? "." : "..", *pos == 0 ? 2 : 3);
        (*pos)++;
        return 1;
    }

    mutex_lock(&t->lock);
    struct tdirent *d = n->entries;
    for (uint64_t k = 2; d && k < *pos; k++) {
        d = d->next;
    }
    int got = d != nullptr;
    if (d) {
        out->ino  = d->node->ino;
        out->type = DT_OF(d->node->mode);
        memcpy(out->name, d->name, d->len + 1);
        (*pos)++;
    }
    mutex_unlock(&t->lock);
    return got;
}

static int make(struct vnode *dir, const char *name, size_t len, uint32_t mode,
                uint32_t uid, uint32_t gid, struct tnode **out) {
    struct tmpfs *t = tfs_of(dir);
    struct tnode *d = node_of(dir);
    if (find(d, name, len)) {
        return -EEXIST;
    }
    struct tnode *n = node_new(t, mode, uid, gid);
    if (!n) {
        return -ENOSPC;
    }
    if (add(d, name, len, n) != 0) {
        n->nlink = 0;
        node_maybe_free(t, n);
        return -ENOSPC;
    }
    n->parent = d;
    *out      = n;
    return 0;
}

static int t_create(struct vnode *dir, const char *name, size_t len,
                    uint32_t mode, uint64_t rdev, uint32_t uid, uint32_t gid,
                    uint64_t *ino) {
    struct tmpfs *t = tfs_of(dir);
    struct tnode *n;
    mutex_lock(&t->lock);
    int err = make(dir, name, len, mode, uid, gid, &n);
    if (!err) {
        n->rdev = rdev;
        *ino    = n->ino;
    }
    mutex_unlock(&t->lock);
    return err;
}

static int t_mkdir(struct vnode *dir, const char *name, size_t len,
                   uint32_t mode, uint32_t uid, uint32_t gid) {
    struct tmpfs *t = tfs_of(dir);
    struct tnode *n;
    mutex_lock(&t->lock);
    int err = make(dir, name, len, mode, uid, gid, &n);
    if (!err) {
        dir->nlink++; // new ".."
    }
    mutex_unlock(&t->lock);
    return err;
}

static int t_symlink(struct vnode *dir, const char *name, size_t len,
                     const char *target, size_t tlen, uint32_t uid,
                     uint32_t gid) {
    struct tmpfs *t = tfs_of(dir);
    char         *s = kmalloc(tlen + 1);
    if (!s) {
        return -ENOSPC;
    }
    memcpy(s, target, tlen);
    s[tlen] = '\0';
    struct tnode *n;

    mutex_lock(&t->lock);
    int err = make(dir, name, len, S_IFLNK | 0777, uid, gid, &n);
    if (!err) {
        n->target = s;
        n->size   = tlen;
    } else {
        kfree(s);
    }
    mutex_unlock(&t->lock);
    return err;
}

static int t_link(struct vnode *dir, const char *name, size_t len,
                  struct vnode *vn) {
    struct tmpfs *t = tfs_of(dir);
    mutex_lock(&t->lock);
    int err = find(node_of(dir), name, len)
                  ? -EEXIST
                  : add(node_of(dir), name, len, node_of(vn));
    if (!err) {
        vn->nlink++;
    }
    mutex_unlock(&t->lock);
    return err;
}

static int t_unlink(struct vnode *dir, const char *name, size_t len,
                    struct vnode *victim) {
    struct tmpfs *t = tfs_of(dir);
    mutex_lock(&t->lock);
    int err = take(node_of(dir), name, len) ? 0 : -ENOENT;
    if (!err) {
        victim->nlink--;
    }
    mutex_unlock(&t->lock);
    return err;
}

static int t_rmdir(struct vnode *dir, const char *name, size_t len,
                   struct vnode *victim) {
    struct tmpfs *t = tfs_of(dir);
    mutex_lock(&t->lock);
    int err = node_of(victim)->entries ? -ENOTEMPTY : 0;
    if (!err && take(node_of(dir), name, len)) {
        victim->nlink = 0;
        dir->nlink--;
    }
    mutex_unlock(&t->lock);
    return err;
}

static int t_rename(struct vnode *odir, const char *oname, size_t olen,
                    struct vnode *ndir, const char *nname, size_t nlen,
                    struct vnode *moved, struct vnode *victim) {
    struct tmpfs *t = tfs_of(odir);
    mutex_lock(&t->lock);
    int err = 0;
    if (victim && S_ISDIR(victim->mode) && node_of(victim)->entries) {
        err = -ENOTEMPTY;
    }
    struct tdirent *over =
        victim && !err ? find(node_of(ndir), nname, nlen) : nullptr;
    if (victim && !err && !over) {
        err = -ENOENT;
    } else if (over) {
        over->node = node_of(moved);
        if (S_ISDIR(victim->mode)) {
            victim->nlink = 0;
            ndir->nlink--;
        } else {
            victim->nlink--;
        }
    } else if (!err) {
        err = add(node_of(ndir), nname, nlen, node_of(moved));
    }
    if (!err) {
        take(node_of(odir), oname, olen);
        if (S_ISDIR(moved->mode) && odir != ndir) {
            node_of(moved)->parent = node_of(ndir);
            odir->nlink--;
            ndir->nlink++;
        }
    }
    mutex_unlock(&t->lock);
    return err;
}

static int t_setattr(struct vnode *vn) {
    store(vn);
    return 0;
}

static struct vobj *t_vobj(struct vnode *vn) {
    struct tnode *n = node_of(vn);
    struct tmpfs *t = tfs_of(vn);
    mutex_lock(&t->lock);
    if (!n->data) {
        n->data = vobj_new(nullptr);
    }
    struct vobj *o = n->data ? vobj_ref(n->data) : nullptr;
    mutex_unlock(&t->lock);
    return o;
}

static int t_vget(struct fs *fs, uint64_t ino, struct vnode *vn) {
    struct tmpfs *t = (struct tmpfs *)fs;
    mutex_lock(&t->lock);
    struct tnode *n = t->nodes;
    while (n && n->ino != ino) {
        n = n->next;
    }
    if (n) {
        n->vrefs++;
        vn->mode  = n->mode;
        vn->uid   = n->uid;
        vn->gid   = n->gid;
        vn->nlink = n->nlink;
        vn->size  = n->size;
        vn->rdev  = n->rdev;
        vn->atime = n->atime;
        vn->mtime = n->mtime;
        vn->ctime = n->ctime;
        vn->priv  = n;
    }
    mutex_unlock(&t->lock);
    return n ? 0 : -ENOENT;
}

static void t_vput(struct vnode *vn) {
    struct tmpfs *t = tfs_of(vn);
    mutex_lock(&t->lock);
    struct tnode *n = node_of(vn);
    store(vn);
    n->vrefs--;
    node_maybe_free(t, n);
    mutex_unlock(&t->lock);
}

static const struct vnode_ops tmpfs_vops = {
    .lookup   = t_lookup,
    .read     = t_read,
    .readdir  = t_readdir,
    .readlink = t_readlink,
    .write    = t_write,
    .truncate = t_truncate,
    .create   = t_create,
    .mkdir    = t_mkdir,
    .symlink  = t_symlink,
    .link     = t_link,
    .unlink   = t_unlink,
    .rmdir    = t_rmdir,
    .rename   = t_rename,
    .setattr  = t_setattr,
    .vobj     = t_vobj,
};

static const struct fs_ops tmpfs_ops = {.vget = t_vget, .vput = t_vput};

struct fs *tmpfs_create(uint32_t mode) {
    struct tmpfs *t = kmalloc(sizeof(*t));
    if (!t) {
        return nullptr;
    }
    *t = (struct tmpfs){.fs       = {.ops      = &tmpfs_ops,
                                     .vops     = &tmpfs_vops,
                                     .name     = "tmpfs",
                                     .root_ino = 1},
                        .next_ino = 1};
    mutex_init(&t->lock);
    struct tnode *root = node_new(t, S_IFDIR | (mode & 07777), 0, 0);
    if (!root) {
        kfree(t);
        return nullptr;
    }
    root->parent = root;
    return &t->fs;
}
