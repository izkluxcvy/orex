#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <stdio_impl.h>

struct mem {
    unsigned char *buf;
    size_t size;
    size_t len;
    size_t pos;
    int append, own;
    char **user_buf;
    size_t *user_size;
};

static ssize_t mem_read(FILE *f, unsigned char *buf, size_t n) {
    struct mem *m = f->cookie;
    size_t k = m->pos < m->len ? m->len - m->pos : 0;
    if (k > n) {
        k = n;
    }
    memcpy(buf, m->buf + m->pos, k);
    m->pos += k;
    return (ssize_t)k;
}

static ssize_t mem_write(FILE *f, const unsigned char *buf, size_t n) {
    struct mem *m = f->cookie;
    if (m->append) {
        m->pos = m->len;
    }
    if (n > m->size - m->pos) {
        errno = ENOSPC;
        return -1;
    }
    memcpy(m->buf + m->pos, buf, n);
    m->pos += n;
    if (m->pos > m->len) {
        m->len = m->pos;
        if (m->len < m->size) {
            m->buf[m->len] = '\0';
        }
    }
    return 0;
}

static off_t mem_seek_to(struct mem *m, off_t off, int whence, size_t limit) {
    off_t base = whence == SEEK_SET   ? 0
                 : whence == SEEK_CUR ? (off_t)m->pos
                                      : (off_t)m->len;
    if (off < -base || (size_t)(base + off) > limit) {
        errno = EINVAL;
        return -1;
    }
    m->pos = (size_t)(base + off);
    return (off_t)m->pos;
}

static off_t mem_seek(FILE *f, off_t off, int whence) {
    struct mem *m = f->cookie;
    return mem_seek_to(m, off, whence, m->size);
}

static int mem_close(FILE *f) {
    struct mem *m = f->cookie;
    if (m->own) {
        free(m->buf);
    }
    free(m);
    return 0;
}

static FILE *mem_file(struct mem *m, unsigned flags) {
    FILE *f = __file_new(-1, flags | F_NOBUF | F_BUFSET);
    if (!f) {
        return nullptr;
    }
    f->cookie = m;
    f->read = mem_read;
    f->write = mem_write;
    f->seek = mem_seek;
    f->close = mem_close;
    return f;
}

FILE *fmemopen(void *__restrict buf, size_t size, const char *__restrict mode) {
    int oflags;
    int fl = __open_flags(mode, &oflags);
    if (fl < 0) {
        return nullptr;
    }
    if (!size) {
        errno = EINVAL;
        return nullptr;
    }
    struct mem *m = calloc(1, sizeof(*m));
    if (!m) {
        return nullptr;
    }
    m->size = size;
    m->append = *mode == 'a';
    if (!buf) {
        if (!(m->buf = calloc(1, size))) {
            free(m);
            return nullptr;
        }
        m->own = 1;
    } else {
        m->buf = buf;
    }
    if (*mode == 'r') {
        m->len = size;
    } else if (*mode == 'w') {
        m->buf[0] = '\0';
    } else {
        m->len = m->pos = strnlen((char *)buf, size);
    }
    FILE *f = mem_file(m, (unsigned)fl);
    if (!f) {
        mem_close(&(FILE){.cookie = m});
    }
    return f;
}

static void publish(struct mem *m) {
    *m->user_buf = (char *)m->buf;
    *m->user_size = m->pos < m->len ? m->pos : m->len;
}

static ssize_t dyn_write(FILE *f, const unsigned char *buf, size_t n) {
    struct mem *m = f->cookie;
    size_t need = m->pos + n + 1;
    if (need > m->size) {
        size_t cap = need < 2 * m->size ? 2 * m->size : need;
        unsigned char *grow = realloc(m->buf, cap);
        if (!grow) {
            return -1;
        }
        memset(grow + m->size, 0, cap - m->size);
        m->buf = grow;
        m->size = cap;
    }
    memcpy(m->buf + m->pos, buf, n);
    m->pos += n;
    if (m->pos > m->len) {
        m->len = m->pos;
    }
    m->buf[m->len] = '\0';
    publish(m);
    return 0;
}

static off_t dyn_seek(FILE *f, off_t off, int whence) {
    struct mem *m = f->cookie;
    off_t r = mem_seek_to(m, off, whence, (size_t)-1 / 2);
    if (r >= 0) {
        publish(m);
    }
    return r;
}

static int dyn_close(FILE *f) {
    struct mem *m = f->cookie;
    publish(m);
    free(m);
    return 0;
}

FILE *open_memstream(char **buf, size_t *size) {
    struct mem *m = calloc(1, sizeof(*m));
    if (!m || !(m->buf = calloc(1, 1))) {
        free(m);
        return nullptr;
    }
    m->size = 1;
    m->user_buf = buf;
    m->user_size = size;
    publish(m);
    FILE *f = mem_file(m, F_NORD);
    if (!f) {
        free(m->buf);
        free(m);
        return nullptr;
    }
    f->write = dyn_write;
    f->seek = dyn_seek;
    f->close = dyn_close;
    return f;
}
