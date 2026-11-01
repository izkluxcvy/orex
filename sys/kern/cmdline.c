#include <cmdline.h>
#include <printf.h>

static char line[256];

void cmdline_init(const char *s) {
    unsigned i = 0;
    for (; s[i] && i < sizeof(line) - 1; i++) {
        line[i] = s[i];
    }
    line[i] = '\0';
    if (i) {
        printf("cmdline: %s\n", line);
    }
}

const char *cmdline() { return line; }

int cmdline_get(const char *key, char *out, unsigned size) {
    int found = 0;
    for (const char *p = line; *p;) {
        while (*p == ' ') {
            p++;
        }
        const char *w = p, *k = key;
        while (*k && *w == *k) {
            w++, k++;
        }
        if (!*k && (*w == '=' || *w == ' ' || !*w)) {
            found = 1;
            unsigned n = 0;
            if (*w == '=') {
                for (w++; *w && *w != ' ' && n + 1 < size; w++) {
                    out[n++] = *w;
                }
            }
            if (size) {
                out[n] = '\0';
            }
        }
        while (*p && *p != ' ') {
            p++;
        }
    }
    return found;
}
