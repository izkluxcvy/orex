#include <string.h>

char *strstr(const char *hay, const char *needle) {
    return memmem(hay, strlen(hay), needle, strlen(needle));
}
