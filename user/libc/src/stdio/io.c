#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stdio_impl.h>

int getc_unlocked(FILE *f) {
    return f->rpos < f->rend ? *f->rpos++ : __uflow(f);
}

int getc(FILE *f) {
    FLOCK(f);
    int c = getc_unlocked(f);
    FUNLOCK(f);
    return c;
}

int fgetc(FILE *f) { return getc(f); }
int getchar() { return getc(stdin); }
int getchar_unlocked() { return getc_unlocked(stdin); }

int putc_unlocked(int c, FILE *f) {
    if (f->wpos < f->wend && (unsigned char)c != f->lbf) {
        return *f->wpos++ = (unsigned char)c;
    }
    return __overflow(f, c);
}

int putc(int c, FILE *f) {
    FLOCK(f);
    int r = putc_unlocked(c, f);
    FUNLOCK(f);
    return r;
}

int fputc(int c, FILE *f) { return putc(c, f); }
int putchar(int c) { return putc(c, stdout); }
int putchar_unlocked(int c) { return putc_unlocked(c, stdout); }

int __lockfile(FILE *f) {
    int tid = __pthread_self()->tid;
    if (f->lock_owner == tid) {
        return 0;
    }
    __lock(&f->lock);
    f->lock_owner = tid;
    return 1;
}

void __unlockfile(FILE *f) {
    f->lock_owner = 0;
    __unlock(&f->lock);
}

void flockfile(FILE *f) {
    if (!__lockfile(f)) {
        f->lock_count++;
    }
}

int ftrylockfile(FILE *f) {
    int tid = __pthread_self()->tid;
    if (f->lock_owner == tid) {
        f->lock_count++;
        return 0;
    }
    if (__sync_val_compare_and_swap(&f->lock, 0, 1) != 0) {
        return -1;
    }
    f->lock_owner = tid;
    return 0;
}

void funlockfile(FILE *f) {
    if (f->lock_count) {
        f->lock_count--;
    } else {
        __unlockfile(f);
    }
}

static int ungetc_unlocked(int c, FILE *f) {
    if (c == EOF || __toread(f) != 0 || f->rpos == f->buf - UNGET) {
        return EOF;
    }
    *--f->rpos = (unsigned char)c;
    f->flags &= ~(unsigned)F_EOF;
    return (unsigned char)c;
}

int ungetc(int c, FILE *f) {
    FLOCK(f);
    int r = ungetc_unlocked(c, f);
    FUNLOCK(f);
    return r;
}

static size_t fread_unlocked(void *__restrict p, size_t size, size_t n,
                             FILE *__restrict f) {
    size_t total;
    if (!size || !n || __builtin_mul_overflow(size, n, &total)) {
        return 0;
    }
    unsigned char *dst = p;
    size_t left = total;
    if (__toread(f) != 0) {
        return 0;
    }
    while (left) {
        size_t have = (size_t)(f->rend - f->rpos);
        if (have) {
            size_t k = have < left ? have : left;
            memcpy(dst, f->rpos, k);
            f->rpos += k;
            dst += k;
            left -= k;
            continue;
        }

        if (f->flags & F_EOF) {
            break;
        }
        if (left >= f->bufsize) {
            ssize_t r = f->read(f, dst, left);
            if (r <= 0) {
                f->flags |= r ? F_ERR : F_EOF;
                break;
            }
            dst += r;
            left -= (size_t)r;
            continue;
        }

        int c = __uflow(f);
        if (c == EOF) {
            break;
        }
        *dst++ = (unsigned char)c;
        left--;
    }
    return (total - left) / size;
}

size_t fread(void *__restrict p, size_t size, size_t n, FILE *__restrict f) {
    FLOCK(f);
    size_t r = fread_unlocked(p, size, n, f);
    FUNLOCK(f);
    return r;
}

static int put_bytes(FILE *f, const unsigned char *p, size_t n) {
    if (n <= (size_t)(f->wend - f->wpos)) {
        memcpy(f->wpos, p, n);
        f->wpos += n;
        return 0;
    }
    if (__flush(f) != 0) {
        return EOF;
    }
    if (n >= f->bufsize || (f->flags & F_NOBUF)) {
        if (f->write(f, p, n) < 0) {
            f->flags |= F_ERR;
            return EOF;
        }
        return 0;
    }
    memcpy(f->wpos, p, n);
    f->wpos += n;
    return 0;
}

static size_t fwrite_unlocked(const void *__restrict p, size_t size, size_t n,
                              FILE *__restrict f) {
    size_t total;
    if (!size || !n || __builtin_mul_overflow(size, n, &total)) {
        return 0;
    }
    if (!f->wbase && __towrite(f) != 0) {
        return 0;
    }
    const unsigned char *s = p;
    size_t head = 0;
    if (f->flags & F_LBF) {
        for (size_t i = total; i; i--) {
            if (s[i - 1] == '\n') {
                head = i;
                break;
            }
        }
    }
    if (head && (put_bytes(f, s, head) != 0 || __flush(f) != 0)) {
        return 0;
    }
    if (put_bytes(f, s + head, total - head) != 0) {
        return head / size;
    }
    return n;
}

size_t fwrite(const void *__restrict p, size_t size, size_t n,
              FILE *__restrict f) {
    FLOCK(f);
    size_t r = fwrite_unlocked(p, size, n, f);
    FUNLOCK(f);
    return r;
}

static char *fgets_unlocked(char *__restrict s, int n, FILE *__restrict f) {
    if (n <= 0) {
        errno = EINVAL;
        return nullptr;
    }
    char *p = s;
    while (--n > 0) {
        int c = getc_unlocked(f);
        if (c == EOF) {
            if (p == s || (f->flags & F_ERR)) {
                return nullptr;
            }
            break;
        }
        *p++ = (char)c;
        if (c == '\n') {
            break;
        }
    }
    *p = '\0';
    return s;
}

char *fgets(char *__restrict s, int n, FILE *__restrict f) {
    FLOCK(f);
    char *r = fgets_unlocked(s, n, f);
    FUNLOCK(f);
    return r;
}

int fputs(const char *__restrict s, FILE *__restrict f) {
    size_t n = strlen(s);
    return n && fwrite(s, 1, n, f) != n ? EOF : 0;
}

int puts(const char *s) {
    FLOCK(stdout);
    int r = fputs(s, stdout) == EOF || putc('\n', stdout) == EOF ? EOF : 0;
    FUNLOCK(stdout);
    return r;
}

static ssize_t getdelim_unlocked(char **__restrict line, size_t *__restrict cap,
                                 int delim, FILE *__restrict f) {
    if (!line || !cap) {
        errno = EINVAL;
        return -1;
    }
    if (!*line) {
        *cap = 0;
    }
    size_t len = 0;
    while (1) {
        int c = getc_unlocked(f);
        if (c == EOF) {
            if (!len || (f->flags & F_ERR)) {
                return -1;
            }
            break;
        }
        if (len + 2 > *cap) {
            size_t want = *cap < 64 ? 128 : *cap * 2;
            char *grow = realloc(*line, want);
            if (!grow) {
                f->flags |= F_ERR;
                return -1;
            }
            *line = grow;
            *cap = want;
        }
        (*line)[len++] = (char)c;
        if (c == delim) {
            break;
        }
    }
    if (len > SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    (*line)[len] = '\0';
    return (ssize_t)len;
}

ssize_t getdelim(char **__restrict line, size_t *__restrict cap, int delim,
                 FILE *__restrict f) {
    FLOCK(f);
    ssize_t r = getdelim_unlocked(line, cap, delim, f);
    FUNLOCK(f);
    return r;
}

ssize_t getline(char **__restrict line, size_t *__restrict cap,
                FILE *__restrict f) {
    return getdelim(line, cap, '\n', f);
}

void clearerr(FILE *f) {
    FLOCK(f);
    f->flags &= ~(F_EOF | F_ERR);
    FUNLOCK(f);
}
int feof(FILE *f) { return !!(f->flags & F_EOF); }
int ferror(FILE *f) { return !!(f->flags & F_ERR); }

int fileno(FILE *f) {
    if (f->read != __fd_read) {
        errno = EBADF;
        return -1;
    }
    return f->fd;
}
