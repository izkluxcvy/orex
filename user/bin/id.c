#include "../lib/ulib.h"

static void show(const char *name, long id) {
    puts(name);
    putnum(id);
}

int main() {
    gid_t groups[16];
    int   n = getgroups(16, groups);
    show("uid ", getuid());
    show(" euid ", geteuid());
    show(" gid ", getgid());
    show(" egid ", getegid());
    for (int i = 0; i < n; i++) {
        show(i ? "," : " groups ", groups[i]);
    }
    puts("\n");
    return 0;
}
