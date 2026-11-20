#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv, char **envp) {
    printf("hello: pid %d, argc %d\n", getpid(), argc);
    for (int i = 0; i < argc; i++) {
        printf("  argv[%d] = %s\n", i, argv[i]);
    }
    for (int i = 0; envp[i]; i++) {
        printf("  envp[%d] = %s\n", i, envp[i]);
    }

    char buf[8];
    ssize_t n = read(3, buf, sizeof(buf));
    if (n > 0) {
        printf("  inherited fd 3: %.*s...\n", (int)n, buf);
    } else {
        puts("  inherited fd 3: closed");
    }
    return argc;
}
