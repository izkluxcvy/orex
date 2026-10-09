#include "../lib/ulib.h"

static char buf[512];

static void say(const char *what, long n) {
    puts(what);
    putnum(n);
    puts("\n");
}

int main() {
    int fd = open("/etc/motd", O_RDONLY);
    say("init: /etc/motd is descriptor ", fd);
    long n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, (size_t)n);
    }
    say("init: and the offset is now ", lseek(fd, 0, SEEK_CUR));

    lseek(fd, 0, SEEK_SET);
    if (fork() == 0) {
        read(fd, buf, 5);
        _exit(0);
    }
    waitpid(-1, nullptr, 0);
    say("init: the child read 5 bytes, and my offset is ",
        lseek(fd, 0, SEEK_CUR));
    close(fd);

    fd = open("/dev", O_RDONLY);
    n  = getdents64(fd, buf, sizeof(buf));
    puts("init: in /dev:");
    for (long at = 0; at < n;) {
        struct dirent64 *de = (struct dirent64 *)(buf + at);
        puts(" ");
        puts(de->d_name);
        at += de->d_reclen;
    }
    puts("\n");
    close(fd);

    struct stat st;
    stat("/bin/hello", &st);
    say("init: /bin/hello has bytes ", st.st_size);

    chdir("/etc");
    getcwd(buf, sizeof(buf));
    puts("init: now in ");
    puts(buf);
    say(", where motd opens as ", open("motd", O_RDONLY));

    int status;
    if (fork() == 0) {
        dup2(open("/dev/null", O_WRONLY), 1);
        char *argv[] = {"hello", nullptr};
        execve("/bin/hello", argv, argv + 1);
        _exit(127);
    }
    waitpid(-1, &status, 0);
    say("init: hello wrote to /dev/null, and its exit code was ",
        (status >> 8) & 0xff);

    say("init: a write to /dev/full gives ",
        write(open("/dev/full", O_WRONLY), "x", 1));
    say("init: and one to a descriptor not open, ", write(1234, "x", 1));
    return 0;
}
