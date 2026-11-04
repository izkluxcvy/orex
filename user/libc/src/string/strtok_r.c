#include <string.h>

char *strtok_r(char *__restrict s, const char *__restrict sep,
               char **__restrict save) {
    if (!s) {
        s = *save;
    }
    s += strspn(s, sep);
    if (!*s) {
        *save = s;
        return NULL;
    }
    char *end = s + strcspn(s, sep);
    if (*end) {
        *end++ = '\0';
    }
    *save = end;
    return s;
}
