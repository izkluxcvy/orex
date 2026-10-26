#include "../lib/ulib.h"

#define ARGS_MAX 16

static char line[256];

static int split(char *s, char **argv) {
    int argc = 0;
    while (argc < ARGS_MAX - 1) {
        while (*s == ' ' || *s == '\n') {
            *s++ = '\0';
        }
        if (!*s) {
            break;
        }
        argv[argc++] = s;
        while (*s && *s != ' ' && *s != '\n') {
            s++;
        }
    }
    argv[argc] = nullptr;
    return argc;
}

static void run(char **argv) {
    char  path[64] = "/bin/";
    char *envp[]   = {"PATH=/bin", nullptr};
    int   pid      = fork();
    if (pid == 0) {
        setpgid(0, 0);
        tcsetpgrp(0, getpid());
        signal(SIGINT, SIG_DFL);
        signal(22, SIG_DFL);

        // redirect
        for (int i = 0; argv[i]; i++) {
            if ((strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0) &&
                argv[i + 1]) {
                int flags =
                    O_WRONLY | O_CREAT | (argv[i][1] ? O_APPEND : O_TRUNC);
                int fd = open(argv[i + 1], flags, 0666);
                if (fd < 0) {
                    puts(argv[i + 1]);
                    puts(": cannot open\n");
                    _exit(1);
                }
                dup2(fd, 1);
                close(fd);
                argv[i] = nullptr;
                break;
            }
        }
        if (strcmp(argv[0], "as") == 0 && argv[1] && argv[2]) {
            int id = (int)strtol(argv[1], nullptr, 10);
            setgroups(0, nullptr);
            setgid(id);
            setuid(id);
            argv += 2;
        }
        if (strchr(argv[0], '/')) {
            execve(argv[0], argv, envp);
        } else if (strlen(argv[0]) < sizeof(path) - 5) {
            memcpy(path + 5, argv[0], strlen(argv[0]) + 1);
            execve(path, argv, envp);
        }
        puts(argv[0]);
        puts(": not found\n");
        _exit(127);
    }
    setpgid(pid, pid);
    tcsetpgrp(0, pid);
    int status;
    while (1) {
        waitpid(pid, &status, 2);
        tcsetpgrp(0, getpgrp());
        if ((status & 0xff) != 0x7f) {
            break;
        }
        puts("(stopped; Enter lets it go on)\n");
        read(0, line, sizeof(line));
        tcsetpgrp(0, pid);
        kill(-pid, 18);
    }
    if (!WIFEXITED(status)) {
        puts("(signal ");
        putnum(WTERMSIG(status));
        puts(")\n");
    } else if (WEXITSTATUS(status)) {
        puts("(exit ");
        putnum(WEXITSTATUS(status));
        puts(")\n");
    }
}

int main() {
    char *argv[ARGS_MAX];
    signal(SIGINT, SIG_IGN);
    signal(22, SIG_IGN);
    ioctl(0, 0x540e, nullptr);
    while (1) {
        puts("$ ");
        long n = read(0, line, sizeof(line) - 1);
        if (n < 0) {
            continue;
        }
        if (n == 0) {
            puts("(end of input)\n");
            continue;
        }
        line[n] = '\0';
        if (split(line, argv)) {
            run(argv);
        }
    }
}
