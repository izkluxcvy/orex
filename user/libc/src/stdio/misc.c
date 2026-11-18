#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libc.h>
#include <stdio_impl.h>
#include <syscall.h>

static int setvbuf_unlocked(FILE *__restrict f, char *__restrict buf, int mode,
                            size_t size) {
    if (mode != _IOFBF && mode != _IOLBF && mode != _IONBF) {
        errno = EINVAL;
        return -1;
    }
    fflush(f);
    f->rpos = f->rend = f->wbase = f->wpos = f->wend = nullptr;
    if (f->flags & F_OWNBUF) {
        free(f->buf - UNGET);
    }
    f->flags &= ~(unsigned)(F_NOBUF | F_LBF | F_OWNBUF);
    f->buf = nullptr;
    f->bufsize = 0;
    f->lbf = EOF;
    if (mode == _IONBF) {
        f->flags |= F_NOBUF;
    } else {
        if (buf && size > UNGET) {
            f->buf = (unsigned char *)buf + UNGET;
            f->bufsize = size - UNGET;
        } else if (!size) {
            size = BUFSIZ;
        }
        if (!f->buf) {
            unsigned char *p = malloc(UNGET + size);
            if (!p) {
                return -1;
            }
            f->buf = p + UNGET;
            f->bufsize = size;
            f->flags |= F_OWNBUF;
        }
        if (mode == _IOLBF) {
            f->flags |= F_LBF;
            f->lbf = '\n';
        }
    }
    f->flags |= F_BUFSET;
    return 0;
}

int setvbuf(FILE *__restrict f, char *__restrict buf, int mode, size_t size) {
    FLOCK(f);
    int r = setvbuf_unlocked(f, buf, mode, size);
    FUNLOCK(f);
    return r;
}

void setbuf(FILE *__restrict f, char *__restrict buf) {
    setvbuf(f, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

void perror(const char *s) {
    int saved = errno;
    const char *msg = strerror(saved);
    if (s && *s) {
        fputs(s, stderr);
        fputs(": ", stderr);
    }
    fputs(msg, stderr);
    fputc('\n', stderr);
    errno = saved;
}

int remove(const char *path) {
    if (unlink(path) == 0) {
        return 0;
    }
    return errno == EISDIR ? rmdir(path) : -1;
}

int renameat(int olddir, const char *old, int newdir, const char *new) {
    return (int)__syscall_ret(
        __syscall4(SYS_renameat, olddir, (long)old, newdir, (long)new));
}

int rename(const char *old, const char *new) {
    return renameat(AT_FDCWD, old, AT_FDCWD, new);
}

FILE *tmpfile() {
    char path[] = P_tmpdir "/tmpfile.XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        return nullptr;
    }
    unlink(path);
    FILE *f = fdopen(fd, "w+");
    if (!f) {
        close(fd);
    }
    return f;
}

char *tmpnam(char *s) {
    static char buf[L_tmpnam];
    char *out = s ? s : buf;
    for (int tries = 0; tries < 100; tries++) {
        static const char set[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        uint64_t r = __random_u64();
        memcpy(out, P_tmpdir "/tmp.", sizeof(P_tmpdir "/tmp.") - 1);
        char *p = out + sizeof(P_tmpdir "/tmp.") - 1;
        for (int i = 0; i < 6; i++, r /= 62) {
            *p++ = set[r % 62];
        }
        *p = '\0';
        if (access(out, F_OK) != 0 && errno == ENOENT) {
            return out;
        }
    }
    return nullptr;
}

char *ctermid(char *s) {
    static char buf[L_ctermid];
    return strcpy(s ? s : buf, "/dev/tty");
}
