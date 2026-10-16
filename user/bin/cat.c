#include "../lib/ulib.h"

static char buf[4096];

static int copy(int fd) {
    long n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, (size_t)n);
    }
    return n < 0;
}

int main(int argc, char **argv) {
    int bad = 0;
    if (argc < 2) {
        return copy(0);
    }
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            puts("cat: cannnot open ");
            puts(argv[i]);
            puts("\n");
            bad = 1;
            continue;
        }
        bad |= copy(fd);
        close(fd);
    }
    return bad;
}
