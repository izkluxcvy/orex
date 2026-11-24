#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fmt.h>

static void put(const char *s) { write(STDERR_FILENO, s, strlen(s)); }

void __assert_fail(const char *expr, const char *file, int line,
                   const char *func) {
    char num[24];
    num[23] = '\0';
    put("Assertion failed: ");
    put(expr);
    put(" (");
    put(file);
    put(": ");
    put(func);
    put(": ");
    put(__fmt_ulong((unsigned long)line, num + 23));
    put(")\n");
    abort();
}
