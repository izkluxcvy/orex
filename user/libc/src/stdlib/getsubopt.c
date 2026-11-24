#include <stdlib.h>
#include <string.h>

int getsubopt(char **optionp, char *const *tokens, char **valuep) {
    char *s = *optionp;
    char *end = s + strcspn(s, ",");
    *optionp = *end ? end + 1 : end;
    *end = '\0';
    char *eq = strchr(s, '=');
    *valuep = nullptr;
    size_t n = eq ? (size_t)(eq - s) : strlen(s);
    for (int i = 0; tokens[i]; i++) {
        if (!strncmp(tokens[i], s, n) && !tokens[i][n]) {
            if (eq) {
                *valuep = eq + 1;
            }
            return i;
        }
    }
    *valuep = s;
    return -1;
}
