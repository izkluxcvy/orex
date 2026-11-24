#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static size_t up(char *out, size_t len) {
    while (len > 1 && out[len - 1] != '/') {
        len--;
    }
    if (len > 1) {
        len--;
    }
    out[len] = '\0';
    return len;
}

char *realpath(const char *__restrict path, char *__restrict resolved) {
    if (!path) {
        errno = EINVAL;
        return nullptr;
    }
    if (!*path) {
        errno = ENOENT;
        return nullptr;
    }

    char out[PATH_MAX] = "/", rest[PATH_MAX], link[PATH_MAX];
    size_t len = 1;
    size_t plen = strlen(path);
    if (plen >= sizeof(rest)) {
        errno = ENAMETOOLONG;
        return nullptr;
    }
    memcpy(rest, path, plen + 1);
    if (*path != '/') {
        if (!getcwd(out, sizeof(out))) {
            return nullptr;
        }
        len = strlen(out);
    }
    int links = 0;
    struct stat st = {.st_mode = S_IFDIR};
    for (char *p = rest; *p;) {
        while (*p == '/') {
            p++;
        }
        size_t n = strcspn(p, "/");
        if (!n) {
            break;
        }
        char *next = p + n;
        if (!S_ISDIR(st.st_mode)) {
            errno = ENOTDIR;
            return nullptr;
        }
        // .
        if (n == 1 && p[0] == '.') {
            p = next;
            continue;
        }
        // ..
        if (n == 2 && p[0] == '.' && p[1] == '.') {
            len = up(out, len);
            p = next;
            continue;
        }

        if (len + (len > 1) + n >= sizeof(out)) {
            errno = ENAMETOOLONG;
            return nullptr;
        }
        if (len > 1) {
            out[len++] = '/';
        }
        memcpy(out + len, p, n);
        out[len += n] = '\0';
        if (lstat(out, &st) != 0) {
            return nullptr;
        }
        if (!S_ISLNK(st.st_mode)) {
            p = next;
            continue;
        }

        // symlink
        if (++links > SYMLOOP_MAX) {
            errno = ELOOP;
            return nullptr;
        }
        ssize_t k = readlink(out, link, sizeof(link));
        if (k < 0) {
            return nullptr;
        }
        size_t tail = strlen(next);
        if ((size_t)k + tail >= sizeof(rest)) {
            errno = ENAMETOOLONG;
            return nullptr;
        }
        memmove(rest + k, next, tail + 1);
        memcpy(rest, link, (size_t)k);
        p = rest;
        if (link[0] == '/') {
            out[len = 1] = '\0';
        } else {
            len = up(out, len);
        }
        st.st_mode = S_IFDIR;
    }

    if (path[plen - 1] == '/' && stat(out, &st) == 0 && !S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return nullptr;
    }
    if (!resolved) {
        return strdup(out);
    }
    return strcpy(resolved, out);
}
