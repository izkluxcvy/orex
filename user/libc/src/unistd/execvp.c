#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void exec_or_shell(const char *path, char *const argv[]) {
    execve(path, argv, environ);
    if (errno != ENOEXEC) {
        return;
    }
    int argc = 0;
    while (argv[argc]) {
        argc++;
    }
    char *sh[argc + 2];
    sh[0] = "sh";
    sh[1] = (char *)path;
    for (int i = 1; i <= argc; i++) {
        sh[i + 1] = argv[i];
    }
    execve("/bin/sh", sh, environ);
}

int execvp(const char *file, char *const argv[]) {
    if (!*file) {
        errno = ENOENT;
        return -1;
    }
    if (strchr(file, '/')) {
        exec_or_shell(file, argv);
        return -1;
    }
    const char *path = getenv("PATH");
    if (!path) {
        path = "/bin:/usr/bin";
    }
    size_t flen = strlen(file);
    int denied = 0;
    for (const char *p = path;; p++) {
        const char *end = strchr(p, ':');
        size_t dlen = end ? (size_t)(end - p) : strlen(p);
        char buf[PATH_MAX];
        if (dlen + 1 + flen < sizeof(buf)) {
            memcpy(buf, p, dlen);
            size_t k = dlen;
            if (k) {
                buf[k++] = '/';
            }
            memcpy(buf + k, file, flen + 1);
            exec_or_shell(buf, argv);
            if (errno == EACCES) {
                denied = 1;
            } else if (errno != ENOENT && errno != ENOTDIR) {
                return -1;
            }
        }
        if (!end) {
            break;
        }
        p = end;
    }
    errno = denied ? EACCES : ENOENT;
    return -1;
}
