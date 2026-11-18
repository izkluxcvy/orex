#include <errno.h>
#include <limits.h>

#include <stdio_impl.h>

static int seek_unlocked(FILE *f, off_t off, int whence) {
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        errno = EINVAL;
        return -1;
    }
    if (f->wbase && __flush(f) != 0) {
        return -1;
    }
    if (whence == SEEK_CUR && f->rpos) {
        off -= f->rend - f->rpos;
    }
    if (f->seek(f, off, whence) < 0) {
        return -1;
    }
    f->rpos = f->rend = nullptr;
    f->wbase = f->wpos = f->wend = nullptr;
    f->flags &= ~(unsigned)F_EOF;
    return 0;
}

int fseeko(FILE *f, off_t off, int whence) {
    FLOCK(f);
    int r = seek_unlocked(f, off, whence);
    FUNLOCK(f);
    return r;
}

static off_t tell_unlocked(FILE *f) {
    int end = (f->flags & F_APPEND) && f->wpos > f->wbase;
    off_t pos = f->seek(f, 0, end ? SEEK_END : SEEK_CUR);
    if (pos < 0) {
        return -1;
    }
    if (f->rpos) {
        pos -= f->rend - f->rpos;
    } else if (f->wbase) {
        pos += f->wpos - f->wbase;
    }
    return pos;
}

off_t ftello(FILE *f) {
    FLOCK(f);
    off_t r = tell_unlocked(f);
    FUNLOCK(f);
    return r;
}

int fseek(FILE *f, long off, int whence) {
    return fseeko(f, (off_t)off, whence);
}

long ftell(FILE *f) {
    off_t pos = ftello(f);
    if (pos > LONG_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (long)pos;
}

void rewind(FILE *f) {
    FLOCK(f);
    seek_unlocked(f, 0, SEEK_SET);
    f->flags &= ~(unsigned)F_EOF;
    FUNLOCK(f);
}

int fgetpos(FILE *__restrict f, fpos_t *__restrict pos) {
    off_t p = ftello(f);
    if (p < 0) {
        return -1;
    }
    pos->__pos = p;
    return 0;
}

int fsetpos(FILE *f, const fpos_t *pos) {
    return fseeko(f, pos->__pos, SEEK_SET);
}
