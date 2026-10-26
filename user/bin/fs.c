#include "../lib/ulib.h"

static int fail(const char *what, long err) {
    puts(what);
    puts(": error ");
    ;
    putnum(err);
    puts("\n");
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        puts("fs mkdir|rmdir|rm|mv|ln|symlink|mkfifo path [path]\n");
        return 2;
    }
    const char *op = argv[1], *a = argv[2], *b = argc > 3 ? argv[3] : nullptr;
    long        r;
    if (strcmp(op, "mkdir") == 0) {
        r = mkdir(a, 0777);
    } else if (strcmp(op, "rmdir") == 0) {
        r = rmdir(a);
    } else if (strcmp(op, "rm") == 0) {
        r = unlink(a);
    } else if (strcmp(op, "mkfifo") == 0) {
        r = syscall4(133, (long)a, 0010666, 0, 0); // mknod, S_IFIFO
    } else if (!b) {
        return fail(op, -22);
    } else if (strcmp(op, "mv") == 0) {
        r = syscall4(82, (long)a, (long)b, 0, 0); // rename
    } else if (strcmp(op, "ln") == 0) {
        r = syscall4(86, (long)a, (long)b, 0, 0); // link
    } else if (strcmp(op, "symlink") == 0) {
        r = syscall4(88, (long)a, (long)b, 0, 0);
    } else {
        return fail(op, -22);
    }
    return r < 0 ? fail(op, r) : 0;
}
