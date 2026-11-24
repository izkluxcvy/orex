#include <libgen.h>
#include <string.h>

char *basename(char *path) {
    if (!path || !*path) {
        return ".";
    }
    size_t n = strlen(path);
    while (n > 1 && path[n - 1] == '/') {
        path[--n] = '\0';
    }
    char *slash = strrchr(path, '/');
    return slash && slash[1] ? slash + 1 : path;
}

char *dirname(char *path) {
    if (!path || !*path) {
        return ".";
    }
    size_t n = strlen(path);
    while (n > 1 && path[n - 1] == '/') {
        n--;
    }
    while (n && path[n - 1] != '/') {
        n--;
    }
    if (!n) {
        return ".";
    }
    while (n > 1 && path[n - 1] == '/') {
        n--;
    }
    path[n] = '\0';
    return path;
}
