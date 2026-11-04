#include <stdlib.h>
#include <string.h>

char *strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    return d ? memcpy(d, s, n) : NULL;
}

char *strndup(const char *s, size_t max) {
    size_t n = strnlen(s, max);
    char *d = malloc(n + 1);
    if (d) {
        memcpy(d, s, n);
        d[n] = '\0';
    }
    return d;
}
