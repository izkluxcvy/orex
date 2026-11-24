#include <stddef.h>
#include <stdlib.h>

typedef int (*cmp_r)(const void *, const void *, void *);

static void swap(char *a, char *b, size_t size) {
    for (size_t i = 0; i < size; i++) {
        char tmp = a[i];
        a[i] = b[i];
        b[i] = tmp;
    }
}

static void insertion(char *base, size_t n, size_t size, cmp_r cmp, void *arg) {
    for (size_t i = 1; i < n; i++) {
        for (char *p = base + i * size; p > base && cmp(p - size, p, arg) > 0;
             p -= size) {
            swap(p - size, p, size);
        }
    }
}

static void sift(char *base, size_t root, size_t n, size_t size, cmp_r cmp,
                 void *arg) {
    for (size_t child; (child = 2 * root + 1) < n; root = child) {
        if (child + 1 < n &&
            cmp(base + child * size, base + (child + 1) * size, arg) < 0) {
            child++;
        }
        if (cmp(base + root * size, base + child * size, arg) >= 0) {
            return;
        }
        swap(base + root * size, base + child * size, size);
    }
}

static void heapsort(char *base, size_t n, size_t size, cmp_r cmp, void *arg) {
    for (size_t i = n / 2; i-- > 0;) {
        sift(base, i, n, size, cmp, arg);
    }
    while (n > 1) {
        swap(base, base + --n * size, size);
        sift(base, 0, n, size, cmp, arg);
    }
}

static void sort(char *base, size_t n, size_t size, cmp_r cmp, void *arg,
                 int depth) {
    while (n > 16) {
        if (depth-- == 0) {
            heapsort(base, n, size, cmp, arg);
            return;
        }

        char *mid = base + n / 2 * size;
        ;
        char *last = base + (n - 1) * size;
        if (cmp(mid, base, arg) < 0) {
            swap(mid, base, size);
        }
        if (cmp(last, mid, arg) < 0) {
            swap(last, mid, size);
            if (cmp(mid, base, arg) < 0) {
                swap(mid, base, size);
            }
        }
        swap(base, mid, size);

        char *lo = base + size;
        char *hi = last;
        while (1) {
            while (lo <= hi && cmp(lo, base, arg) < 0) {
                lo += size;
            }
            while (lo <= hi && cmp(hi, base, arg) > 0) {
                hi -= size;
            }
            if (lo >= hi) {
                break;
            }
            swap(lo, hi, size);
            lo += size;
            hi -= size;
        }
        swap(base, hi, size);

        size_t left = (size_t)(hi - base) / size;
        size_t right = n - left - 1;
        if (left < right) {
            sort(base, left, size, cmp, arg, depth);
            base = hi + size;
            n = right;
        } else {
            sort(hi + size, right, size, cmp, arg, depth);
            n = left;
        }
    }
    insertion(base, n, size, cmp, arg);
}

void qsort_r(void *base, size_t n, size_t size,
             int (*cmp)(const void *, const void *, void *), void *arg) {
    int depth = 0;
    for (size_t k = n; k; k >>= 1) {
        depth += 2;
    }
    if (size) {
        sort(base, n, size, cmp, arg, depth);
    }
}

static int plain(const void *a, const void *b, void *fn) {
    return ((int (*)(const void *, const void *))fn)(a, b);
}

void qsort(void *base, size_t n, size_t size,
           int (*cmp)(const void *, const void *)) {
    qsort_r(base, n, size, plain, (void *)cmp);
}
