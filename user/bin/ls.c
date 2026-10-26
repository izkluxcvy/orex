#include "../lib/ulib.h"

static char buf[4096];

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    int         fd  = open(dir, O_RDONLY);
    if (fd < 0) {
        puts("ls: cannot open ");
        puts(dir);
        puts("\n");
        return 1;
    }
    if (chdir(dir) != 0) {
        return 1;
    }
    long n;
    while ((n = getdents64(fd, buf, sizeof(buf))) > 0) {
        for (long at = 0; at < n;) {
            struct dirent64 *d = (struct dirent64 *)(buf + at);
            struct stat      st;
            if (syscall4(6, (long)d->d_name, (long)&st, 0, 0) == 0) {
                putnum((long)st.st_ino);
                puts("\t");
                putnum((long)st.st_nlink);
                puts("\t");
                putnum(st.st_mode >> 12 & 017);
                puts("\t");
                putnum(st.st_mode >> 6 & 7);
                putnum(st.st_mode >> 3 & 7);
                putnum(st.st_mode & 7);
                puts("\t");
                putnum(st.st_size);
                puts("\t");
            }
            puts(d->d_name);
            puts("\n");
            at += d->d_reclen;
        }
    }
    return 0;
}
