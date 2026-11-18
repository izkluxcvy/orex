#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdio_impl.h>

int __open_flags(const char *mode, int *oflags) {
    int o;
    unsigned fl;
    switch (*mode) {
    case 'r':
        o = O_RDONLY;
        fl = F_NOWR;
        break;
    case 'w':
        o = O_WRONLY | O_CREAT | O_TRUNC;
        fl = F_NORD;
        break;
    case 'a':
        o = O_WRONLY | O_CREAT | O_APPEND;
        fl = F_NORD | F_APPEND;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    for (const char *m = mode + 1; *m; m++) {
        switch (*m) {
        case '+':
            o = (o & ~O_ACCMODE) | O_RDWR;
            fl &= ~(unsigned)(F_NORD | F_NOWR);
            break;
        case 'x':
            o |= O_EXCL;
            break;
        case 'e':
            o |= O_CLOEXEC;
            break;
        }
    }
    *oflags = o;
    return (int)fl;
}

FILE *fopen(const char *__restrict path, const char *__restrict mode) {
    int oflags;
    int fl = __open_flags(mode, &oflags);
    if (fl < 0) {
        return nullptr;
    }
    int fd = open(path, oflags, 0666);
    if (fd < 0) {
        return nullptr;
    }
    FILE *f = __file_new(fd, (unsigned)fl);
    if (!f) {
        close(fd);
    }
    return f;
}

static int adopt(int fd, int fl, int oflags) {
    int have = fcntl(fd, F_GETFL);
    if (have < 0) {
        return -1;
    }
    int acc = have & O_ACCMODE;
    if ((!(fl & F_NORD) && acc == O_WRONLY) ||
        (!(fl & F_NOWR) && acc == O_RDONLY)) {
        errno = EINVAL;
        return -1;
    }
    if ((fl & F_APPEND) && !(have & O_APPEND) &&
        fcntl(fd, F_SETFL, have | O_APPEND) < 0) {
        return -1;
    }
    if ((oflags & O_CLOEXEC) && fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
        return -1;
    }
    return 0;
}

FILE *fdopen(int fd, const char *mode) {
    int oflags;
    int fl = __open_flags(mode, &oflags);
    if (fl < 0 || adopt(fd, fl, oflags) != 0) {
        return nullptr;
    }
    return __file_new(fd, (unsigned)fl);
}

static void release_buffer(FILE *f) {
    if (f->flags & F_OWNBUF) {
        free(f->buf - UNGET);
    }
    f->buf = nullptr;
    f->bufsize = 0;
    f->rpos = f->rend = f->wbase = f->wpos = f->wend = nullptr;
}

int fclose(FILE *f) {
    FLOCK(f);
    int ret = fflush(f);
    if (f->close && f->close(f) != 0) {
        ret = EOF;
    }
    release_buffer(f);
    FUNLOCK(f);
    __file_unlink(f);
    if (f->flags & F_PERM) {
        f->flags |= F_NORD | F_NOWR;
    } else {
        free(f);
    }
    return ret;
}

FILE *freopen(const char *__restrict path, const char *__restrict mode,
              FILE *__restrict f) {
    int oflags;
    int fl = __open_flags(mode, &oflags);
    if (fl < 0) {
        goto fail;
    }
    if (!path) {
        if (f->read != __fd_read || adopt(f->fd, fl, oflags) != 0) {
            goto fail;
        }
    } else {
        int fd = open(path, oflags, 0666);
        if (fd < 0) {
            goto fail;
        }
        if (f->close) {
            f->close(f);
        }
        if (fd != f->fd && f->read == __fd_read) {
            if (dup2(fd, f->fd) < 0) {
                close(fd);
                goto fail;
            }
            close(fd);
            fd = f->fd;
        }
        f->fd = fd;
        f->read = __fd_read;
        f->write = __fd_write;
        f->seek = __fd_seek;
        f->close = __fd_close;
        f->cookie = nullptr;
    }
    unsigned perm = f->flags & F_PERM;
    release_buffer(f);
    f->flags = (unsigned)fl | perm;
    f->lbf = EOF;
    f->orientation = 0;
    return f;

fail:
    fclose(f);
    return nullptr;
}
