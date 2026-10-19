#include <stddef.h>
#include <stdint.h>

#include <cred.h>
#include <errno.h>
#include <proc.h>
#include <uaccess.h>

#define KEEP ((long)(uint32_t)-1)

static struct cred *cur() { return &curproc()->cred; }

static int is_root() { return cur()->euid == 0; }

int cred_in_group(const struct cred *c, gid_t gid, int real) {
    if (gid == (real ? c->rgid : c->egid)) {
        return 1;
    }
    for (int i = 0; i < c->ngroups; i++) {
        if (c->groups[i] == gid) {
            return 1;
        }
    }
    return 0;
}

long sys_getuid() { return cur()->ruid; }
long sys_geteuid() { return cur()->euid; }
long sys_getgid() { return cur()->rgid; }
long sys_getegid() { return cur()->egid; }

static int held(uint32_t id, uint32_t r, uint32_t e, uint32_t s) {
    return id == r || id == e || id == s;
}

static long set_res(uint32_t *r, uint32_t *e, uint32_t *s, long nr, long ne,
                    long ns) {
    nr = (uint32_t)nr, ne = (uint32_t)ne, ns = (uint32_t)ns;
    if (!is_root()) {
        if ((nr != KEEP && !held(nr, *r, *e, *s)) ||
            (ne != KEEP && !held(ne, *r, *e, *s)) ||
            (ns != KEEP && !held(ns, *r, *e, *s))) {
            return -EPERM;
        }
    }
    if (nr != KEEP) {
        *r = (uint32_t)nr;
    }
    if (ne != KEEP) {
        *e = (uint32_t)ne;
    }
    if (ns != KEEP) {
        *s = (uint32_t)ns;
    }
    return 0;
}

long sys_setresuid(long ruid, long euid, long suid) {
    struct cred *c = cur();
    return set_res(&c->ruid, &c->euid, &c->suid, ruid, euid, suid);
}

long sys_setresgid(long rgid, long egid, long sgid) {
    struct cred *c = cur();
    return set_res(&c->rgid, &c->egid, &c->sgid, rgid, egid, sgid);
}

long sys_setuid(long uid) {
    struct cred *c = cur();
    if (is_root()) {
        c->ruid = c->euid = c->suid = (uid_t)uid;
        return 0;
    }
    if ((uid_t)uid != c->ruid && (uid_t)uid != c->suid) {
        return -EPERM;
    }
    c->euid = (uid_t)uid;
    return 0;
}

long sys_setgid(long gid) {
    struct cred *c = cur();
    if (is_root()) {
        c->rgid = c->egid = c->sgid = (gid_t)gid;
        return 0;
    }
    if ((gid_t)gid != c->rgid && (gid_t)gid != c->sgid) {
        return -EPERM;
    }
    c->egid = (gid_t)gid;
    return 0;
}

static long set_re(uint32_t *r, uint32_t *e, uint32_t *s, long nr, long ne) {
    uint32_t old_r = *r;
    nr = (uint32_t)nr, ne = (uint32_t)ne;
    if (!is_root()) {
        if ((nr != KEEP && nr != *r && nr != *e) ||
            (nr != KEEP && !held((uint32_t)ne, *r, *e, *s))) {
            return -EPERM;
        }
    }
    if (nr != KEEP) {
        *r = (uint32_t)nr;
    }
    if (ne != KEEP) {
        *e = (uint32_t)ne;
    }
    if (nr != KEEP || (ne != KEEP && (uint32_t)ne != old_r)) {
        *s = *e;
    }
    return 0;
}

long sys_setreuid(long ruid, long euid) {
    struct cred *c = cur();
    return set_re(&c->ruid, &c->euid, &c->suid, ruid, euid);
}

long sys_setregid(long rgid, long egid) {
    struct cred *c = cur();
    return set_re(&c->rgid, &c->egid, &c->sgid, rgid, egid);
}

static long get_res(uint32_t r, uint32_t e, uint32_t s, uint32_t *ur,
                    uint32_t *ue, uint32_t *us) {
    int err = copy_to_user(ur, &r, 4);
    if (!err) {
        err = copy_to_user(ue, &e, 4);
    }
    if (!err) {
        err = copy_to_user(us, &s, 4);
    }
    return err;
}

long sys_getresuid(uid_t *r, uid_t *e, uid_t *s) {
    struct cred *c = cur();
    return get_res(c->ruid, c->euid, c->suid, r, e, s);
}

long sys_getresgid(gid_t *r, gid_t *e, gid_t *s) {
    struct cred *c = cur();
    return get_res(c->rgid, c->egid, c->sgid, r, e, s);
}

long sys_getgroups(long size, gid_t *list) {
    struct cred *c = cur();
    if (size == 0) {
        return c->ngroups;
    }
    if (size < c->ngroups) {
        return -EINVAL;
    }
    int err = copy_to_user(list, c->groups, (size_t)c->ngroups * sizeof(gid_t));
    return err ? err : c->ngroups;
}

long sys_setgroups(long size, const gid_t *list) {
    if (!is_root()) {
        return -EPERM;
    }
    if (size < 0 || size > NGROUPS_MAX) {
        return -EINVAL;
    }
    struct cred *c = cur();
    gid_t        groups[NGROUPS_MAX];
    if (size &&
        copy_from_user(groups, list, (size_t)size * sizeof(gid_t)) != 0) {
        return -EFAULT;
    }
    for (long i = 0; i < size; i++) {
        c->groups[i] = groups[i];
    }
    c->ngroups = (int)size;
    return 0;
}
