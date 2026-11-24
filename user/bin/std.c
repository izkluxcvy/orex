#include <assert.h>
#include <libgen.h>
#include <limits.h>
#include <search.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int by_string(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void show(const void *node, VISIT v, int depth) {
    if (v == postorder || v == leaf) {
        printf(" %s(%d)", *(char *const *)node, depth);
    }
}

static jmp_buf again;

static void deep(int n) {
    if (n == 0) {
        longjmp(again, 42);
    }
    deep(n - 1);
}

int main(int argc, char **argv) {
    void *root = nullptr;
    for (int i = 1; i < argc; i++) {
        tsearch(argv[i], &root, (int (*)(const void *, const void *))strcmp);
    }
    printf("tree:");
    twalk(root, show);
    printf("\n");

    qsort(argv + 1, (size_t)argc - 1, sizeof(*argv), by_string);
    printf("sorted:");
    for (int i = 1; i < argc; i++) {
        printf(" %s", argv[i]);
    }
    char *key = "orex";
    char **hit =
        bsearch(&key, argv + 1, (size_t)argc - 1, sizeof(*argv), by_string);
    printf("\norex is %s\n", hit ? "found" : "not found");

    int r = setjmp(again);
    printf("setjmp returned %d\n", r);
    if (!r) {
        deep(100);
    }

    char buf[PATH_MAX];
    char path[] = "/bin/../etc/./passwd";
    printf("realpath %s = %s\n", path, realpath(path, buf) ? buf : "(error)");
    char a[] = "/usr/lib/";
    char b[] = "/usr/lib/";
    printf("basename %s, dirname %s\n", basename(a), dirname(b));
    char name[SIG2STR_MAX];
    sig2str(SIGTERM, name);
    printf("signal %d is SIG%s\n", SIGTERM, name);
    assert(argc > 1);
    return 0;
}
