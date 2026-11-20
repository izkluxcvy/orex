#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdio_impl.h>

static int finish(int err, size_t count) {
    if (err) {
        return -1;
    }
    if (count > INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)count;
}

/*
 * to file
 */

struct file_ctx {
    FILE *f;
    char buf[512];
    size_t n;
};

static int file_write(struct sink *s, const char *p, size_t n) {
    struct file_ctx *c = s->ctx;
    if (!(c->f->flags & F_NOBUF)) {
        return fwrite(p, 1, n, c->f) == n ? 0 : -1;
    }
    if (c->n + n > sizeof(c->buf)) {
        if (c->n && fwrite(c->buf, 1, c->n, c->f) != c->n) {
            return -1;
        }
        c->n = 0;
        if (n > sizeof(c->buf)) {
            return fwrite(p, 1, n, c->f) == n ? 0 : -1;
        }
    }
    memcpy(c->buf + c->n, p, n);
    c->n += n;
    return 0;
}

int vfprintf(FILE *__restrict f, const char *__restrict fmt, va_list ap) {
    struct file_ctx c = {.f = f};
    struct sink s = {.write = file_write, .ctx = &c};
    FLOCK(f);
    if (!f->wbase && __towrite(f) != 0) {
        FUNLOCK(f);
        return -1;
    }
    int err = __vprintf_core(&s, fmt, ap);
    if (c.n && fwrite(c.buf, 1, c.n, f) != c.n) {
        err = -1;
    }
    FUNLOCK(f);
    return finish(err, s.count);
}

int vprintf(const char *__restrict fmt, va_list ap) {
    return vfprintf(stdout, fmt, ap);
}

int fprintf(FILE *__restrict f, const char *__restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(f, fmt, ap);
    va_end(ap);
    return r;
}

int printf(const char *__restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return r;
}

/*
 * to memory
 */

struct str_ctx {
    char *buf;
    size_t cap, used;
};

static int str_write(struct sink *s, const char *p, size_t n) {
    struct str_ctx *c = s->ctx;
    size_t room = c->cap ? c->cap - 1 - c->used : 0;
    size_t k = n < room ? n : room;
    memcpy(c->buf + c->used, p, k);
    c->used += k;
    return 0;
}

int vsnprintf(char *__restrict buf, size_t n, const char *__restrict fmt,
              va_list ap) {
    struct str_ctx c = {.buf = buf, .cap = n};
    struct sink s = {.write = str_write, .ctx = &c};
    int err = __vprintf_core(&s, fmt, ap);
    if (n) {
        buf[c.used] = '\0';
    }
    return finish(err, s.count);
}

int vsprintf(char *__restrict buf, const char *__restrict fmt, va_list ap) {
    return vsnprintf(buf, SIZE_MAX, fmt, ap);
}

int snprintf(char *__restrict buf, size_t n, const char *__restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char *__restrict buf, const char *__restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, SIZE_MAX, fmt, ap);
    va_end(ap);
    return r;
}

int vasprintf(char **__restrict out, const char *__restrict fmt, va_list ap) {
    va_list aq;
    va_copy(aq, ap);
    int n = vsnprintf(nullptr, 0, fmt, aq);
    va_end(aq);
    if (n < 0 || !(*out = malloc((size_t)n + 1))) {
        return -1;
    }
    return vsnprintf(*out, (size_t)n + 1, fmt, ap);
}

int asprintf(char **__restrict out, const char *__restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vasprintf(out, fmt, ap);
    va_end(ap);
    return r;
}

/*
 * to descriptor
 */

struct fd_ctx {
    int fd;
    char buf[512];
    size_t n;
};

static int fd_flush(struct fd_ctx *c) {
    for (size_t at = 0; at < c->n;) {
        ssize_t w = write(c->fd, c->buf + at, c->n - at);
        if (w < 0) {
            return -1;
        }
        at += (size_t)w;
    }
    c->n = 0;
    return 0;
}

static int fd_write(struct sink *s, const char *p, size_t n) {
    struct fd_ctx *c = s->ctx;
    while (n) {
        size_t k = sizeof(c->buf) - c->n < n ? sizeof(c->buf) - c->n : n;
        memcpy(c->buf + c->n, p, k);
        c->n += k;
        p += k;
        n -= k;
        if (c->n == sizeof(c->buf) && fd_flush(c)) {
            return -1;
        }
    }
    return 0;
}

int vdprintf(int fd, const char *__restrict fmt, va_list ap) {
    struct fd_ctx c = {.fd = fd};
    struct sink s = {.write = fd_write, .ctx = &c};
    int err = __vprintf_core(&s, fmt, ap);
    if (fd_flush(&c)) {
        err = -1;
    }
    return finish(err, s.count);
}

int dprintf(int fd, const char *__restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vdprintf(fd, fmt, ap);
    va_end(ap);
    return r;
}
