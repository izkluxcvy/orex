#include "../lib/ulib.h"

static void show(const char *what, const char *s) {
    puts(what);
    puts(s);
    puts("\n");
}

int main() {
    char *shared = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    char *own    = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memcpy(shared, "parent's", 9);
    memcpy(own, "parent's", 9);

    if (fork() == 0) {
        memcpy(shared, "child's", 8);
        memcpy(own, "child's", 8);
        _exit(0);
    }
    waitpid(-1, nullptr, 0);
    show("share: MAP_SHARED after the child wrote: ", shared);
    show("share: MAP_PRIVATE after the child wrote: ", own);

    int fd = open("/tmp/mapped", O_RDWR | O_CREAT | O_TRUNC, 0644);
    write(fd, "0123456789\n", 11);
    char *file = mmap(nullptr, 11, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memcpy(file, "abc", 3);
    syscall4(26, (long)file, 11, 4, 0); // msync
    char buf[16] = {0};
    lseek(fd, 0, SEEK_SET);
    read(fd, buf, 11);
    show("share: read back from the file: ", buf);
    return 0;
}
