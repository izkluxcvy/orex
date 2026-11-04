#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <unistd.h>

#include "libc.h"

static char **owned;
static size_t nowned;
static char **our_environ;

static void forget(char *s) {
    for (size_t i = 0; i < nowned; i++) {
        if (owned[i] == s) {
            owned[i] = owned[--nowned];
            free(s);
            return;
        }
    }
}

static size_t count(void) {
    size_t n = 0;
    while (environ && environ[n]) {
        n++;
    }
    return n;
}

static int valid(const char *name) {
    if (!name || !*name || strchr(name, '=')) {
        errno = EINVAL;
        return 0;
    }
    return 1;
}

static char **find(const char *name, size_t len) {
    for (char **e = environ; e && *e; e++) {
        if (!strncmp(*e, name, len) && (*e)[len] == '=') {
            return e;
        }
    }
    return NULL;
}

static int install(char *s, size_t len) {
    char **e = find(s, len);
    if (e) {
        forget(*e);
        *e = s;
        return 0;
    }
    size_t n = count();
    char **grow = realloc(environ == our_environ ? our_environ : NULL,
                          (n + 2) * sizeof(char *));
    if (!grow) {
        return -1;
    }
    if (environ != our_environ && n) {
        memcpy(grow, environ, n * sizeof(char *));
    }
    grow[n] = s;
    grow[n + 1] = NULL;
    environ = our_environ = grow;
    return 0;
}

int setenv(const char *name, const char *value, int overwrite) {
    if (!valid(name)) {
        return -1;
    }
    size_t len = strlen(name);
    if (!overwrite && find(name, len)) {
        return 0;
    }
    size_t vlen = strlen(value);
    char *s = malloc(len + vlen + 2);
    char **own = reallocarray(owned, nowned + 1, sizeof(char *));
    if (!s || !own) {
        free(s);
        owned = own ? own : owned;
        return -1;
    }
    owned = own;
    memcpy(s, name, len);
    s[len] = '=';
    memcpy(s + len + 1, value, vlen + 1);
    owned[nowned++] = s;
    if (install(s, len) != 0) {
        forget(s);
        return -1;
    }
    return 0;
}

int putenv(char *s) {
    char *eq = strchr(s, '=');
    if (!eq || eq == s) {
        errno = EINVAL;
        return -1;
    }
    return install(s, (size_t)(eq - s));
}

int unsetenv(const char *name) {
    if (!valid(name)) {
        return -1;
    }
    size_t len = strlen(name);
    for (char **e; (e = find(name, len));) {
        forget(*e);
        do {
            e[0] = e[1];
        } while (*e++);
    }
    return 0;
}

int clearenv(void) {
    while (nowned) {
        free(owned[--nowned]);
    }
    free(our_environ);
    our_environ = NULL;
    environ = NULL;
    return 0;
}

char *secure_getenv(const char *name) {
    return getauxval(AT_SECURE) ? NULL : getenv(name);
}
