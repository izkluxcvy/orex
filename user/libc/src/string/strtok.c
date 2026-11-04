#include <string.h>

char *strtok(char *__restrict s, const char *__restrict sep) {
    static char *save;
    return strtok_r(s, sep, &save);
}
