#include "../lib/ulib.h"

static char buf[4096];

static void say(const char *what, long n) {
    puts(what);
    putnum(n);
    puts("\n");
}

int main() {
    int fds[2];
    pipe(fds);
    say("init: the pipe's reading end is ", fds[0]);
    say("init: the pipe's writing end is ", fds[1]);

    if (fork() == 0) {
        dup2(fds[1], 1);
        close(fds[0]);
        close(fds[1]);
        char *argv[] = {"hello", "through", "a", "pipe", nullptr};
        execve("/bin/hello", argv, argv + 4);
        _exit(127);
    }
    close(fds[1]);

    long n, total = 0;
    while ((n = read(fds[0], buf, sizeof(buf))) > 0) {
        for (long i = 0; i < n; i++) {
            if (buf[i] >= 'a' && buf[i] <= 'z') {
                buf[i] -= 'a' - 'A';
            }
        }
        write(1, buf, (size_t)n);
        total += n;
    }
    say("init: read a total of ", total);
    close(fds[0]);
    waitpid(-1, nullptr, 0);

    pipe(fds);
    if (fork() == 0) {
        close(fds[0]);
        long sent = 0;
        for (int i = 0; i < 64; i++) {
            sent += write(fds[1], buf, sizeof(buf));
        }
        say("writer: sent ", sent);
        _exit(0);
    }
    close(fds[1]);
    total = 0;
    while ((n = read(fds[0], buf, 1000)) > 0) {
        total += n;
    }
    waitpid(-1, nullptr, 0);
    say("init: received ", total);

    pipe(fds);
    close(fds[0]);
    say("init: a write with no reader gives ", write(fds[1], "x", 1));
    return 0;
}
