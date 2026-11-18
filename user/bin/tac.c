#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tac(FILE *in, FILE *out) {
    char *all = nullptr, *line = nullptr;
    size_t size = 0, cap = 0;
    FILE *mem = open_memstream(&all, &size);
    if (!mem) {
        return -1;
    }
    ssize_t n;
    while ((n = getline(&line, &cap, in)) > 0) {
        fwrite(line, 1, (size_t)n, mem);
        if (line[n - 1] != '\n') {
            fputc('\n', mem);
        }
    }
    free(line);
    fclose(mem);

    for (size_t end = size; end;) {
        size_t start = end - 1;
        while (start && all[start - 1] != '\n') {
            start--;
        }
        fwrite(all + start, 1, end - start, out);
        end = start;
    }
    free(all);
    return ferror(in) ? -1 : 0;
}

int main(int argc, char **argv) {
    int status = 0;
    if (argc < 2) {
        return tac(stdin, stdout) ? 1 : 0;
    }
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "r");
        if (!f) {
            perror(argv[i]);
            status = 1;
            continue;
        }
        if (tac(f, stdout)) {
            perror(argv[i]);
            status = 1;
        }
        fclose(f);
    }
    return status;
}
