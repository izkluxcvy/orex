#include "../lib/ulib.h"

static void say(const char *what, long n) {
    puts(what);
    putnum(n);
    puts("\n");
}

static volatile int got;

static void on_usr1(int sig) { got = sig; }

static void on_segv(int sig) {
    puts("child: caught signal ");
    putnum(sig);
    puts(", and leaves by itself\n");
    _exit(3);
}

static void report(const char *who, int pid) {
    int status;
    waitpid(pid, &status, 0);
    puts(who);
    say(WIFEXITED(status) ? ": exit " : ": signal ",
        WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status));
}

int main() {
    signal(SIGUSR1, on_usr1);
    int pid = fork();
    if (pid == 0) {
        kill(getppid(), SIGUSR1);
        _exit(0);
    }
    while (!got) {
    }
    say("init: the handler ran, for signal ", got);
    report("the sender", pid);

    pid = fork();
    if (pid == 0) {
        while (1) {
        }
    }
    kill(pid, SIGTERM);
    report("a loop sent SIGTERM", pid);

    pid = fork();
    if (pid == 0) {
        signal(SIGTERM, SIG_IGN);
        kill(getpid(), SIGTERM);
        _exit(0);
    }
    report("one that ignroes SIGTERM", pid);

    pid = fork();
    if (pid == 0) {
        *(volatile int *)8 = 1;
    }
    report("a bad pointer", pid);
    pid = fork();
    if (pid == 0) {
        signal(SIGSEGV, on_segv);
        *(volatile int *)8 = 1;
    }
    report("the same, with a handler", pid);

    unsigned long set = SIGBIT(SIGUSR1), old;
    got               = 0;
    sigprocmask(SIG_BLOCK, &set, &old);
    kill(getpid(), SIGUSR1);
    say("init: SIGUSR1 sent while blocked; the handler saw ", got);
    sigprocmask(SIG_SETMASK, &old, nullptr);
    say("init: and once unblocked, ", got);

    int status;
    pid = fork();
    if (pid == 0) {
        while (1) {
        }
    }
    kill(pid, 19);
    waitpid(pid, &status, 2);
    say("init: the stopped child's status (0x137f is 4991): ", status);
    kill(pid, SIGKILL);
    report("stopped, then sent SIGKILL", pid);

    int fds[2];
    pipe(fds);
    close(fds[0]);
    pid = fork();
    if (pid == 0) {
        write(fds[1], "x", 1);
        _exit(0);
    }
    report("a writer to a pipe with no reader", pid);
    return 0;
}
