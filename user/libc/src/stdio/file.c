#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdio_impl.h>
#include <syscall.h>

ssize_t __fd_read(FILE *f, unsigned char *buf, size_t n) {
    return __syscall_ret(__syscall3(SYS_read, f->fd, (long)buf, (long)n));
}

ssize_t __fd_write(FILE *f, const unsigned char *buf, size_t n) {
    while (n) {
        ssize_t w =
            __syscall_ret(__syscall3(SYS_write, f->fd, (long)buf, (long)n));
        if (w < 0) {
            return -1;
        }
        buf += w;
        n -= (size_t)w;
    }
    return 0;
}

off_t __fd_seek(FILE *f, off_t off, int whence) {
    return lseek(f->fd, off, whence);
}

int __fd_close(FILE *f) {
    return (int)__syscall_ret(__syscall1(SYS_close, f->fd));
}

#define STD(name, fdno, fl, nextp, prevp)                                      \
    static FILE name = {.flags = (fl) | F_PERM,                                \
                        .fd = (fdno),                                          \
                        .lbf = EOF,                                            \
                        .read = __fd_read,                                     \
                        .write = __fd_write,                                   \
                        .seek = __fd_seek,                                     \
                        .close = __fd_close,                                   \
                        .next = (nextp),                                       \
                        .prev = (prevp)}

static FILE f_stdin, f_stdout, f_stderr;
STD(f_stdin, 0, F_NOWR, &f_stdout, nullptr);
STD(f_stdout, 1, F_NORD, &f_stderr, &f_stdin);
STD(f_stderr, 2, F_NORD | F_NOBUF | F_BUFSET, nullptr, &f_stdout);

FILE *const stdin = &f_stdin;
FILE *const stdout = &f_stdout;
FILE *const stderr = &f_stderr;

static FILE *head = &f_stdin;
static volatile int ofl;

void __ofl_lock() { __lock(&ofl); }
void __ofl_unlock() { __unlock(&ofl); }

void __file_link(FILE *f) {
    __ofl_lock();
    f->prev = nullptr;
    f->next = head;
    if (head) {
        head->prev = f;
    }
    head = f;
    __ofl_unlock();
}

void __file_unlink(FILE *f) {
    __ofl_lock();
    if (f->prev) {
        f->prev->next = f->next;
    } else if (head == f) {
        head = f->next;
    }
    if (f->next) {
        f->next->prev = f->prev;
    }
    f->prev = f->next = nullptr;
    __ofl_unlock();
}

FILE *__file_new(int fd, unsigned flags) {
    FILE *f = calloc(1, sizeof(*f));
    if (!f) {
        return nullptr;
    }
    f->fd = fd;
    f->flags = flags;
    f->lbf = EOF;
    f->read = __fd_read;
    f->write = __fd_write;
    f->seek = __fd_seek;
    f->close = __fd_close;
    __file_link(f);
    return f;
}

static void setup(FILE *f) {
    if (f->flags & F_BUFSET) {
        if (!f->buf) {
            f->buf = f->tiny + UNGET;
            f->bufsize = 1;
        }
        return;
    }

    f->flags |= F_BUFSET;
    unsigned char *p = malloc(UNGET + BUFSIZ);
    if (!p) {
        f->flags |= F_NOBUF;
        f->buf = f->tiny + UNGET;
        f->bufsize = 1;
        return;
    }
    f->flags |= F_OWNBUF;
    f->buf = p + UNGET;
    f->bufsize = BUFSIZ;
    if (f->read == __fd_read && isatty(f->fd)) {
        f->flags |= F_LBF;
        f->lbf = '\n';
    }
}

int __flush(FILE *f) {
    if (f->wpos > f->wbase) {
        ssize_t r = f->write(f, f->wbase, (size_t)(f->wpos - f->wbase));
        f->wpos = f->wbase;
        if (r < 0) {
            f->flags |= F_ERR;
            return EOF;
        }
    }
    return 0;
}

int __toread(FILE *f) {
    if (f->flags & F_NORD) {
        f->flags |= F_ERR;
        errno = EBADF;
        return EOF;
    }

    setup(f);
    if (f->wbase) {
        if (__flush(f) != 0) {
            return EOF;
        }
        f->wbase = f->wpos = f->wend = nullptr;
    }
    if (!f->rpos) {
        f->rpos = f->rend = f->buf;
    }
    return 0;
}

int __towrite(FILE *f) {
    if (f->flags & F_NOWR) {
        f->flags |= F_ERR;
        errno = EBADF;
        return EOF;
    }

    setup(f);
    if (f->rpos < f->rend) {
        f->seek(f, -(off_t)(f->rend - f->rpos), SEEK_CUR);
    }
    f->rpos = f->rend = nullptr;
    f->wbase = f->wpos = f->buf;
    f->wend = (f->flags & F_NOBUF) ? f->buf : f->buf + f->bufsize;
    return 0;
}

int __overflow(FILE *f, int c) {
    unsigned char ch = (unsigned char)c;
    if (!f->wbase && __towrite(f) != 0) {
        return EOF;
    }
    if (f->flags & F_NOBUF) {
        if (f->write(f, &ch, 1) < 0) {
            f->flags |= F_ERR;
            return EOF;
        }
        return ch;
    }
    if (f->wpos == f->wend && __flush(f) != 0) {
        return EOF;
    }
    *f->wpos++ = ch;
    if (ch == f->lbf && __flush(f) != 0) {
        return EOF;
    }
    return ch;
}

static void flush_line_buffered(FILE *f) {
    __ofl_lock();
    for (FILE *g = head; g; g = g->next) {
        if (g == f || !(g->flags & F_LBF) || g->wpos <= g->wbase) {
            continue;
        }
        if (!__libc_threads.threaded) {
            __flush(g);
        } else if (ftrylockfile(g) == 0) {
            __flush(g);
            funlockfile(g);
        }
    }
    __ofl_unlock();
}

int __uflow(FILE *f) {
    if (__toread(f) != 0) {
        return EOF;
    }
    if (f->rpos < f->rend) {
        return *f->rpos++;
    }
    if (f->flags & F_EOF) {
        return EOF;
    }
    if (f->flags & (F_LBF | F_NOBUF)) {
        flush_line_buffered(f);
    }
    ssize_t n = f->read(f, f->buf, f->bufsize);
    f->rpos = f->rend = f->buf;
    if (n <= 0) {
        f->flags |= n ? F_ERR : F_EOF;
        return EOF;
    }
    f->rend += n;
    return *f->rpos++;
}

static int flush_one(FILE *f) {
    if (f->wbase) {
        return __flush(f);
    }
    if (f->rpos < f->rend &&
        f->seek(f, -(off_t)(f->rend - f->rpos), SEEK_CUR) >= 0) {
        f->rpos = f->rend = nullptr;
    }
    return 0;
}

int fflush(FILE *f) {
    if (!f) {
        int ret = 0;
        __ofl_lock();
        for (FILE *g = head; g; g = g->next) {
            FLOCK(g);
            ret |= flush_one(g);
            FUNLOCK(g);
        }
        __ofl_unlock();
        return ret;
    }

    FLOCK(f);
    int ret = flush_one(f);
    FUNLOCK(f);
    return ret;
}

void __stdio_fork_child() {
    ofl = 0;
    for (FILE *g = head; g; g = g->next) {
        g->lock = g->lock_owner = g->lock_count = 0;
    }
}

void __stdio_exit() { fflush(nullptr); }
