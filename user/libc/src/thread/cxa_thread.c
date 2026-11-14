#include <stdlib.h>

#include <libc.h>

struct dtor {
    void (*fn)(void *);
    void *obj;
    struct dtor *next;
};

static _Thread_local struct dtor *dtors;

int __cxa_thread_atexit_impl(void (*fn)(void *), void *obj, void *dso) {
    (void)dso;
    struct dtor *d = malloc(sizeof(*d));
    if (!d) {
        return -1;
    }
    *d = (struct dtor){fn, obj, dtors};
    dtors = d;
    return 0;
}

void __cxa_thread_finalize() {
    while (dtors) {
        struct dtor *d = dtors;
        dtors = d->next;
        d->fn(d->obj);
        free(d);
    }
}
