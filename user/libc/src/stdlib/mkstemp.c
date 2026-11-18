#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libc.h>

#define TRIES 100

static int fill(char *tmpl) {
    size_t len = strlen(tmpl);
    if (len < 6 || strcmp(tmpl + len - 6, "XXXXXX")) {
        errno = EINVAL;
        return -1;
    }
    static const char set[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    uint64_t r = __random_u64();
    for (char *p = tmpl + len - 6; *p; p++, r /= 62) {
        *p = set[r % 62];
    }
    return 0;
}

int mkostemp(char *tmpl, int flags) {
    char *x = tmpl + strlen(tmpl) - 6;
    for (int i = 0; i < TRIES; i++) {
        if (fill(tmpl) != 0) {
            return -1;
        }
        int fd =
            open(tmpl, O_RDWR | O_CREAT | O_EXCL | (flags & ~O_ACCMODE), 0600);
        if (fd >= 0 || errno != EEXIST) {
            return fd;
        }
        memcpy(x, "XXXXXX", 6);
    }
    return -1;
}

int mkstemp(char *tmpl) { return mkostemp(tmpl, 0); }

char *mkdtemp(char *tmpl) {
    char *x = tmpl + strlen(tmpl) - 6;
    for (int i = 0; i < TRIES; i++) {
        if (fill(tmpl) != 0) {
            return nullptr;
        }
        if (mkdir(tmpl, 0700) == 0) {
            return tmpl;
        }
        if (errno != EEXIST) {
            return nullptr;
        }
        memcpy(x, "XXXXXX", 6);
    }
    return nullptr;
}
