#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

static void say(const char *s) { write(1, s, strlen(s)); }

static void hex(unsigned long v) {
    char buf[16], *p = buf + sizeof(buf);
    do {
        *--p = "0123456789abcdef"[v & 15];
    } while (v >>= 4);
    write(1, p, (size_t)(buf + sizeof(buf) - p));
}

static void show(const char *s) {
    mbstate_t st = {0};
    size_t n = strlen(s);
    while (n) {
        wchar_t wc;
        size_t k = mbrtowc(&wc, s, n, &st);
        if (k == (size_t)-1 || k == (size_t)-2) {
            say("  (not a character here)\n");
            return;
        }
        char buf[4];
        say("  ");
        if (wc < 0x80 || MB_CUR_MAX > 1) {
            write(1, buf, wcrtomb(buf, wc, NULL));
        }
        say(" U+");
        hex((unsigned long)wc);
        say(" width ");
        int w = wcwidth(wc);
        w < 0 ? say("-1") : hex((unsigned long)w);
        say(iswalpha(wc) ? " alpha" : "");
        say(iswupper(wc) ? " upper" : "");
        say(iswlower(wc) ? " lower" : "");
        say(iswpunct(wc) ? " punct" : "");
        say(iswspace(wc) ? " space" : "");
        if (towupper(wc) != (wint_t)wc) {
            say(" -> ");
            write(1, buf, wcrtomb(buf, (wchar_t)towupper(wc), NULL));
        }
        say("\n");
        s += k;
        n -= k;
    }
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        say("C:\n");
        show(argv[i]);
        say(setlocale(LC_ALL, "C.UTF-8"));
        say(":\n");
        show(argv[i]);
        setlocale(LC_ALL, "C");
    }
    return 0;
}
