#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <kmalloc.h>
#include <memcmp.h>
#include <memcpy.h>
#include <memset.h>
#include <printf.h>
#include <ramfs.h>
#include <vfs.h>

#define NEWC_HEADER 110

struct rnode {
    const char    *path;
    const char    *name;
    size_t         name_len;
    uint32_t       mode;
    uint32_t       uid, gid;
    uint32_t       mtime;
    const uint8_t *data;
    size_t         size;
    size_t         parent, child, sibling;
    uint32_t       cino, nlink;
    size_t         canon;
};

struct ramfs {
    struct fs     fs;
    struct rnode *nodes;
    size_t        count;
};

static uint32_t hex8(const char *s) {
    uint32_t v = 0;
    for (int i = 0; i < 8; i++) {
        char c = s[i];
        v = v * 16 + (uint32_t)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
    }
    return v;
}

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static size_t align4(size_t v) { return (v + 3) & ~(size_t)3; }

static const char *strip(const char *path) {
    while (path[0] == '.' && path[1] == '/') {
        path += 2;
    }
    return path;
}

static size_t scan(const uint8_t *base, size_t size, struct rnode *out) {
    size_t n = 0, off = 0;

    while (off + NEWC_HEADER <= size) {
        const char *h = (const char *)base + off;
        if (h[0] != '0' || h[1] != '7' || h[2] != '0' || h[3] != '7' ||
            h[4] != '0' || h[5] != '1') {
            printf("ramfs: bad cpio magic at %lu\n", (unsigned long)off);
            break;
        }
        uint32_t mode     = hex8(h + 14);
        uint32_t uid      = hex8(h + 22);
        uint32_t gid      = hex8(h + 30);
        uint32_t mtime    = hex8(h + 46);
        uint32_t cino     = hex8(h + 6);
        uint32_t nlink    = hex8(h + 38);
        uint32_t filesize = hex8(h + 54);
        uint32_t namesize = hex8(h + 94);

        size_t name_off = off + NEWC_HEADER;
        size_t data_off = align4(name_off + namesize);
        if (namesize == 0 || data_off > size || filesize > size - data_off) {
            break;
        }

        const char *name = (const char *)base + name_off;
        if (streq(name, "TRAILER!!!")) {
            break;
        }
        if (out) {
            out[n] = (struct rnode){.path  = strip(name),
                                    .mode  = mode,
                                    .uid   = uid,
                                    .gid   = gid,
                                    .mtime = mtime,
                                    .data  = base + data_off,
                                    .size  = filesize,
                                    .cino  = cino,
                                    .nlink = nlink,
                                    .canon = n};
        }
        n++;
        off = align4(data_off + filesize);
    }
    return n;
}

static void join_links(struct rnode *nodes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        struct rnode *n = &nodes[i];
        if (S_ISDIR(n->mode) || n->nlink < 2 || n->canon != i) {
            continue;
        }
        size_t with_data = i;
        for (size_t j = i + 1; j < count; j++) {
            if (nodes[j].cino == n->cino && nodes[j].nlink == n->nlink &&
                !S_ISDIR(nodes[j].mode) && nodes[j].size) {
                with_data = j;
            }
        }
        for (size_t j = i; j < count; j++) {
            if (nodes[j].cino == n->cino && nodes[j].nlink == n->nlink &&
                !S_ISDIR(nodes[j].mode)) {
                nodes[j].canon = with_data;
            }
        }
    }
}

static void link_tree(struct rnode *nodes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        struct rnode *n     = &nodes[i];
        const char   *slash = nullptr;
        for (const char *c = n->path; *c; c++) {
            if (*c == '/') {
                slash = c;
            }
        }
        n->name     = slash ? slash + 1 : n->path;
        n->name_len = 0;
        while (n->name[n->name_len]) {
            n->name_len++;
        }

        size_t plen = slash ? (size_t)(slash - n->path) : 0;
        n->parent   = 0;
        for (size_t j = 1; plen && j < count; j++) {
            const char *p = nodes[j].path;
            size_t      k = 0;
            while (k < plen && p[k] == n->path[k]) {
                k++;
            }
            if (k == plen && p[k] == '\0') {
                n->parent = j;
                break;
            }
        }
        n->sibling             = nodes[n->parent].child;
        nodes[n->parent].child = i;
    }
}

static struct rnode *node_of(struct vnode *vn) { return vn->priv; }

static int ramfs_lookup(struct vnode *dir, const char *name, size_t len,
                        uint64_t *ino) {
    struct ramfs *rfs = (struct ramfs *)dir->fs;
    if (len == 2 && name[0] == '.' && name[1] == '.') {
        *ino = node_of(dir)->parent + 1;
        return 0;
    }

    for (size_t i = node_of(dir)->child; i; i = rfs->nodes[i].sibling) {
        struct rnode *n = &rfs->nodes[i];
        if (n->name_len == len && memcmp(n->name, name, len) == 0) {
            *ino = n->canon + 1;
            return 0;
        }
    }
    return -ENOENT;
}

static long ramfs_read(struct vnode *vn, void *buf, size_t n, uint64_t off) {
    struct rnode *node = node_of(vn);
    if (off >= node->size) {
        return 0;
    }
    if (n > node->size - off) {
        n = node->size - off;
    }
    memcpy(buf, node->data + off, n);
    return (long)n;
}

static long ramfs_readlink(struct vnode *vn, char *buf, size_t n) {
    return ramfs_read(vn, buf, n, 0);
}

static int ramfs_readdir(struct vnode *dir, uint64_t *pos,
                         struct vfs_dirent *out) {
    struct ramfs *rfs  = (struct ramfs *)dir->fs;
    struct rnode *self = node_of(dir);

    if (*pos < 2) {
        size_t idx   = *pos == 0 ? (size_t)(self - rfs->nodes) : self->parent;
        out->ino     = idx + 1;
        out->type    = DT_DIR;
        out->name[0] = '.';
        out->name[1] = *pos == 0 ? '\0' : '.';
        out->name[2] = '\0';
        (*pos)++;
        return 1;
    }

    size_t i = self->child;
    for (uint64_t k = 2; i && k < *pos; k++) {
        i = rfs->nodes[i].sibling;
    }
    if (!i) {
        return 0;
    }
    struct rnode *n = &rfs->nodes[i];
    out->ino        = n->canon + 1;
    out->type       = DT_OF(n->mode);
    memcpy(out->name, n->name, n->name_len);
    out->name[n->name_len] = '\0';
    (*pos)++;
    return 1;
}

static int ramfs_vget(struct fs *fs, uint64_t ino, struct vnode *vn) {
    struct ramfs *rfs = (struct ramfs *)fs;
    if (ino == 0 || ino > rfs->count) {
        return -ENOENT;
    }
    struct rnode *n = &rfs->nodes[ino - 1];
    vn->mode        = n->mode;
    vn->uid         = n->uid;
    vn->gid         = n->gid;
    vn->nlink       = S_ISDIR(n->mode) ? 2 : n->nlink ? n->nlink : 1;
    vn->size        = n->size;
    vn->mtime       = n->mtime;
    vn->atime       = n->mtime;
    vn->ctime       = n->mtime;
    vn->priv        = n;
    return 0;
}

static const struct vnode_ops ramfs_vops = {
    .lookup   = ramfs_lookup,
    .read     = ramfs_read,
    .readdir  = ramfs_readdir,
    .readlink = ramfs_readlink,
};

static const struct fs_ops ramfs_ops = {.vget = ramfs_vget};

struct fs *ramfs_create(const void *archive, size_t size) {
    size_t count = scan(archive, size, nullptr);
    if (count == 0) {
        return nullptr;
    }

    struct ramfs *rfs   = kmalloc(sizeof(*rfs));
    struct rnode *nodes = kmalloc(count * sizeof(*nodes));
    if (!rfs || !nodes) {
        kfree(rfs);
        kfree(nodes);
        return nullptr;
    }
    scan(archive, size, nodes);

    if (!streq(nodes[0].path, ".") || !S_ISDIR(nodes[0].mode)) {
        printf("ramfs: archive does not start with \".\"\n");
        kfree(rfs);
        kfree(nodes);
        return nullptr;
    }
    nodes[0].child = 0;
    join_links(nodes, count);
    link_tree(nodes, count);

    *rfs = (struct ramfs){.fs    = {.ops      = &ramfs_ops,
                                    .vops     = &ramfs_vops,
                                    .name     = "ramfs",
                                    .root_ino = 1},
                          .nodes = nodes,
                          .count = count};
    printf("ramfs: %lu entries\n", (unsigned long)count);
    return &rfs->fs;
}
