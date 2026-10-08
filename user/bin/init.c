#include "../lib/ulib.h"

static int counter = 100;

static void say(const char *who, const char *what, long n) {
    puts(who);
    puts(what);
    putnum(n);
    puts("\n");
}

int main() {
    say("init: ", "pid ", getpid());

    int pid = fork();
    if (pid == 0) {
        counter++;
        say("child: ", "pid ", getpid());
        say("child: ", "my counter is ", counter);
        char *argv[] = {"hello", "again", nullptr};
        char *envp[] = {"PATH=/bin", nullptr};
        execve("/bin/hello", argv, envp);
        puts("child: no /bin/hello\n");
        _exit(127);
    }
    int status;
    int got = waitpid(pid, &status, 0);
    say("init: ", "waited for ", got);
    say("init: ", "its exit code was ", (status >> 8) & 0xff);
    say("init: ", "my counter is ", counter);

    pid = fork();
    if (pid == 0) {
        *(volatile int *)8 = 1;
        _exit(0);
    }
    waitpid(pid, &status, 0);
    say("init: ", "the second was killed by signal ", status & 0x7f);

    pid = fork();
    if (pid == 0) {
        if (fork() == 0) {
            while (getppid() != 1) {
                syscall4(24, 0, 0, 0, 0); // yield
            }
            say("orphan: ", "my parent is now ", getppid());
            _exit(7);
        }
        _exit(0);
    }
    while ((got = waitpid(-1, &status, 0)) > 0) {
        say("init: ", "reaped ", got);
    }
    return 0;
}
