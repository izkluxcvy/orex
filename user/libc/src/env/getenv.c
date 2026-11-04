#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *getenv(const char *name) {
    size_t n = strlen(name);
    if (!n || strchr(name, '=')) {
        return nullptr;
    }
    for (char **e = environ; e && *e; e++) {
        if (!strncmp(*e, name, n) && (*e)[n] == '=') {
            return *e + n + 1;
        }
    }
    return nullptr;
}
