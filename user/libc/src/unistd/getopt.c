#include <string.h>
#include <unistd.h>

char *optarg;
int optind = 1, opterr = 1, optopt;

static int pos;
static int last_ind = 1;

static void complain(const char *prog, const char *msg, char c) {
    if (!opterr) {
        return;
    }
    char tail[3] = {c, '\n', '\0'};
    write(STDERR_FILENO, prog, strlen(prog));
    write(STDERR_FILENO, msg, strlen(msg));
    write(STDERR_FILENO, tail, 2);
}

int getopt(int argc, char *const argv[], const char *opts) {
    if (optind == 0) {
        optind = 1, pos = 0;
    }
    if (optind != last_ind) {
        pos = 0;
    }
    int quiet = opts[0] == ':';
    if (pos == 0) {
        const char *a = optind < argc ? argv[optind] : NULL;
        if (!a || a[0] != '-' || !a[1]) {
            return -1;
        }
        if (a[1] == '-' && !a[2]) {
            last_ind = ++optind;
            return -1;
        }
        pos = 1;
    }

    const char *a = argv[optind];
    char c = a[pos++];
    const char *spec = c != ':' ? strchr(opts + quiet, c) : NULL;
    if (!a[pos]) {
        optind++, pos = 0;
    }
    int ret = c;
    if (!spec) {
        optopt = c;
        if (!quiet) {
            complain(argv[0], ": illegal option -- ", c);
        }
        ret = '?';
    } else if (spec[1] == ':') {
        if (pos) {
            optarg = (char *)a + pos;
            optind++, pos = 0;
        } else if (optind < argc) {
            optarg = argv[optind++];
        } else {
            optopt = c;
            if (!quiet) {
                complain(argv[0], ": option requires an argument -- ", c);
            }
            ret = quiet ? ':' : '?';
        }
    }
    last_ind = optind;
    return ret;
}
