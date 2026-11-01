#include <blk.h>
#include <devfs.h>
#include <errno.h>
#include <ext2.h>
#include <memcmp.h>
#include <mutex.h>
#include <proc.h>
#include <tmpfs.h>
#include <uaccess.h>
#include <vfs.h>

#define MS_RDONLY  1
#define MS_REMOUNT 32

static int is(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++, b++;
    }
    return *a == *b;
}

static struct mutex mount_mutex;

static long mount_(const char *usrc, const char *utarget, const char *utype,
                   unsigned long flags, const void *data) {
    (void)data;
    if (curproc()->cred.euid != 0) {
        return -EPERM;
    }
    char target[256], type[32] = "", src[256] = "";
    long err = copy_str_from_user(target, utarget, sizeof(target));
    if (err < 0) {
        return err;
    }
    if (flags & MS_REMOUNT) {
        return vfs_remount(target, (flags & MS_RDONLY) != 0);
    }
    if ((err = copy_str_from_user(type, utype, sizeof(type))) < 0 ||
        (usrc && (err = copy_str_from_user(src, usrc, sizeof(src))) < 0)) {
        return err;
    }
    struct fs *fs = nullptr;
    if (is(type, "ext2")) {
        struct vnode *vn;
        if ((err = vfs_lookup(nullptr, src, VFS_FOLLOW, &vn)) != 0) {
            return err;
        }
        struct blkdev *dev = devfs_blkdev(vn);
        vnode_put(vn);
        if (!dev) {
            return -ENOTBLK;
        }
        if (vfs_blk_mounted(dev)) {
            return -EBUSY;
        }
        blk_invalidate(dev);
        if (!(fs = ext2_mount(dev, (flags & MS_RDONLY) != 0))) {
            return -EINVAL;
        }
    } else if (is(type, "tmpfs")) {
        fs = tmpfs_create(01777);
    } else if (is(type, "devfs")) {
        fs = devfs_create();
    } else {
        return -ENODEV;
    }
    if (!fs) {
        return -ENOMEM;
    }
    if ((err = vfs_mount(target, fs)) != 0 && fs->ops->destroy) {
        fs->ops->destroy(fs);
    }
    return err;
}

long sys_mount(const char *usrc, const char *utarget, const char *utype,
               unsigned long flags, const void *data) {
    mutex_lock(&mount_mutex);
    long err = mount_(usrc, utarget, utype, flags, data);
    mutex_unlock(&mount_mutex);
    return err;
}

long sys_umount2(const char *utarget, int flags) {
    (void)flags;
    if (curproc()->cred.euid != 0) {
        return -EPERM;
    }
    char target[256];
    long err = copy_str_from_user(target, utarget, sizeof(target));
    if (err >= 0) {
        mutex_lock(&mount_mutex);
        err = vfs_umount(target);
        mutex_unlock(&mount_mutex);
    }
    return err;
}
