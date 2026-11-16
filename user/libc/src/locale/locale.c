#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include <locale_impl.h>

struct __locale_struct __global_locale = {
    {"C", "C", "C", "C", "C", "C"},
    0,
};

static const char *const cat_names[LC_ALL] = {
    "LC_CTYPE",   "LC_NUMERIC",  "LC_TIME",
    "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES",
};

static volatile int lock;

static void take() {
    while (__atomic_exchange_n(&lock, 1, __ATOMIC_ACQUIRE)) {
        __builtin_ia32_pause();
    }
}

static void give() { __atomic_store_n(&lock, 0, __ATOMIC_RELEASE); }

static int is_utf8(const char *cs, size_t n) {
    char b[8];
    size_t k = 0;
    for (size_t i = 0; i < n && k < sizeof(b); i++) {
        if (cs[i] != '-' && cs[i] != '_') {
            b[k++] = (char)(cs[i] | 0x20);
        }
    }
    return k == 4 && !memcmp(b, "utf8", 4) && n <= 6;
}

int __locale_kind(const char *name) {
    size_t n = strlen(name);
    if (!n || n >= LOCALE_NAME_MAX || strchr(name, '/') || name[0] == '.') {
        return 0;
    }
    if (!strcmp(name, "C") || !strcmp(name, "POSIX")) {
        return 1;
    }

    const char *dot = strchr(name, '.');
    const char *at = strchr(name, '@');
    if (at && dot && at < dot) {
        return 0;
    }
    if (dot && !is_utf8(dot + 1, (size_t)((at ? at : name + n) - dot - 1))) {
        return 0;
    }
    return 2;
}

static const char *from_env(int cat) {
    const char *v = getenv("LC_ALL");
    if (!v || !*v) {
        v = getenv(cat_names[cat]);
    }
    if (!v || !*v) {
        v = getenv("LANG");
    }
    return v && *v ? v : "C";
}

static void set_name(struct __locale_struct *l, int cat, const char *name) {
    strcpy(l->name[cat], name);
    if (cat == LC_CTYPE) {
        l->utf8 = __locale_kind(name) == 2;
    }
}

static int resolve(int mask, const char *name,
                   char want[LC_ALL][LOCALE_NAME_MAX]) {
    for (int c = 0; c < LC_ALL; c++) {
        if (!(mask & (1 << c))) {
            continue;
        }
        const char *n = *name ? name : from_env(c);
        if (!__locale_kind(n)) {
            return 0;
        }
        strcpy(want[c], n);
    }
    return 1;
}

static char *all_name(struct __locale_struct *l, char *buf) {
    int same = 1;
    for (int c = 1; c < LC_ALL; c++) {
        same &= !strcmp(l->name[c], l->name[0]);
    }
    if (same) {
        return strcpy(buf, l->name[0]);
    }
    buf[0] = 0;
    for (int c = 0; c < LC_ALL; c++) {
        strcat(buf, cat_names[c]);
        strcat(buf, "=");
        strcat(buf, l->name[c]);
        if (c < LC_ALL - 1) {
            strcat(buf, ";");
        }
    }
    return buf;
}

static int split_all(const char *name, char want[LC_ALL][LOCALE_NAME_MAX]) {
    int seen = 0;
    for (const char *p = name; *p;) {
        const char *eq = strchr(p, '=');
        const char *end = strchr(p, ';');
        if (!end) {
            end = p + strlen(p);
        }
        if (!eq || eq > end) {
            return 0;
        }
        int c = 0;
        while (c < LC_ALL && (strlen(cat_names[c]) != (size_t)(eq - p) ||
                              memcmp(cat_names[c], p, (size_t)(eq - p)))) {
            c++;
        }
        size_t n = (size_t)(end - eq - 1);
        if (c == LC_ALL || n >= LOCALE_NAME_MAX) {
            return 0;
        }
        memcpy(want[c], eq + 1, n);
        want[c][n] = 0;
        if (!__locale_kind(want[c])) {
            return 0;
        }
        seen |= 1 << c;
        p = *end ? end + 1 : end;
    }
    return seen == LC_ALL_MASK;
}

char *setlocale(int cat, const char *name) {
    static char buf[LC_ALL * (LOCALE_NAME_MAX + 16)];
    if (cat < 0 || cat > LC_ALL) {
        return nullptr;
    }
    take();
    char *r = nullptr;
    if (name) {
        char want[LC_ALL][LOCALE_NAME_MAX];
        int mask = cat == LC_ALL ? LC_ALL_MASK : 1 << cat;
        int ok = cat == LC_ALL && strchr(name, '=') ? split_all(name, want)
                                                    : resolve(mask, name, want);
        if (!ok) {
            give();
            return nullptr;
        }
        for (int c = 0; c < LC_ALL; c++) {
            if (mask & (1 << c)) {
                set_name(&__global_locale, c, want[c]);
            }
        }
    }
    r = cat == LC_ALL ? all_name(&__global_locale, buf)
                      : strcpy(buf, __global_locale.name[cat]);
    give();
    return r;
}

locale_t newlocale(int mask, const char *name, locale_t base) {
    char want[LC_ALL][LOCALE_NAME_MAX];
    if ((mask & ~LC_ALL_MASK) || !name || !resolve(mask, name, want)) {
        errno = EINVAL;
        return nullptr;
    }
    locale_t l = base && base != LC_GLOBAL_LOCALE ? base : malloc(sizeof(*l));
    if (!l) {
        return nullptr;
    }
    if (l != base) {
        if (base == LC_GLOBAL_LOCALE) {
            take();
            *l = __global_locale;
            give();
        } else {
            *l = (struct __locale_struct){{"C", "C", "C", "C", "C", "C"}, 0};
        }
    }
    for (int c = 0; c < LC_ALL; c++) {
        if (mask & (1 << c)) {
            set_name(l, c, want[c]);
        }
    }
    return l;
}

locale_t duplocale(locale_t loc) {
    locale_t l = malloc(sizeof(*l));
    if (l) {
        take();
        *l = *__real_locale(loc);
        give();
    }
    return l;
}

void freelocale(locale_t loc) {
    if (loc && loc != LC_GLOBAL_LOCALE && loc != &__global_locale) {
        free(loc);
    }
}

locale_t uselocale(locale_t loc) {
    struct pthread *self = __pthread_self();
    locale_t old = self->locale ? self->locale : LC_GLOBAL_LOCALE;
    if (loc) {
        self->locale = loc == LC_GLOBAL_LOCALE ? nullptr : loc;
    }
    return old;
}

const char *getlocalename_l(int cat, locale_t loc) {
    if (cat < 0 || cat >= LC_ALL) {
        return nullptr;
    }
    return __real_locale(loc)->name[cat];
}

struct lconv *localeconv() {
    static struct lconv c = {
        ".",      "",       "",       "",       "",       "",
        "",       "",       "",       "",       CHAR_MAX, CHAR_MAX,
        CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX,
        CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX,
    };
    return &c;
}

size_t __ctype_get_mb_cur_max() { return UTF8 ? 4 : 1; }
