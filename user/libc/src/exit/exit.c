#include <stdlib.h>

#include <libc.h>
#include <pthread_impl.h>

#ifdef SHARED
__attribute__((visibility("hidden"))) void __dl_run_fini();
#endif

__attribute__((weak)) void __stdio_exit() {}

extern void (*const __fini_array_start[])()
    __attribute__((weak, visibility("hidden")));
extern void (*const __fini_array_end[])()
    __attribute__((weak, visibility("hidden")));

#define STATIC_FNS 32

struct fn {
    union {
        void (*plain)();
        void (*with_arg)(void *);
    } f;
    void *arg, *dso;
    int has_arg, done;
};

struct fns {
    struct fn fn[STATIC_FNS];
    int n;
    struct fns *older;
};

static struct fns first, quick;
static struct fns *atexit_fns = &first;
static volatile int lock;

static int add(struct fns **list, struct fn fn) {
    __lock(&lock);
    struct fns *f = *list;
    if (f->n == STATIC_FNS) {
        if (list != &atexit_fns || !(f = calloc(1, sizeof(*f)))) {
            __unlock(&lock);
            return -1;
        }
        f->older = *list;
        *list = f;
    }
    f->fn[f->n++] = fn;
    __unlock(&lock);
    return 0;
}

static void run(struct fns **list) {
    while (1) {
        __lock(&lock);
        struct fns *f = *list;
        while (f->n == 0 && f->older) {
            f = *list = f->older;
        }
        if (f->n == 0) {
            __unlock(&lock);
            return;
        }
        struct fn fn = f->fn[--f->n];
        __unlock(&lock);
        if (!fn.done) {
            fn.has_arg ? fn.f.with_arg(fn.arg) : fn.f.plain();
        }
    }
}

int atexit(void (*fn)()) {
    return add(&atexit_fns, (struct fn){.f.plain = fn});
}

int __cxa_atexit(void (*fn)(void *), void *arg, void *dso) {
    return add(
        &atexit_fns,
        (struct fn){.f.with_arg = fn, .arg = arg, .dso = dso, .has_arg = 1});
}

void cxa_finalize(void *dso) {
    while (1) {
        __lock(&lock);
        struct fn *found = nullptr;
        for (struct fns *f = atexit_fns; f && !found; f = f->older) {
            for (int i = f->n; i-- > 0;) {
                if (!f->fn[i].done && (!dso || f->fn[i].dso == dso)) {
                    found = &f->fn[i];
                    break;
                }
            }
        }
        struct fn fn = found ? *found : (struct fn){0};
        if (found) {
            found->done = 1;
        }
        __unlock(&lock);
        if (!found) {
            return;
        }
        fn.has_arg ? fn.f.with_arg(fn.arg) : fn.f.plain();
    }
}

int at_quick_exit(void (*fn)()) {
    struct fns *q = &quick;
    return add(&q, (struct fn){.f.plain = fn});
}

void exit(int status) {
    if (__cxa_thread_finalize) {
        __cxa_thread_finalize();
    }
    run(&atexit_fns);
#ifdef SHARED
    __dl_run_fini();
#else
    for (size_t i = (size_t)(__fini_array_end - __fini_array_start); i-- > 0;) {
        __fini_array_start[i]();
    }
#endif
    __stdio_exit();
    _Exit(status);
}

void quick_exit(int status) {
    struct fns *q = &quick;
    run(&q);
    _Exit(status);
}
