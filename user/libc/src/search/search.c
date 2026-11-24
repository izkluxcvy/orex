#include <errno.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>

/*
 * hash
 */

static ENTRY *table;
static size_t size, used;

int hcreate(size_t nel) {
    if (table) {
        return 0;
    }
    size = 8;
    while (size < nel + nel / 3 + 1 && size < ((size_t)1 << 40)) {
        size <<= 1;
    }
    table = calloc(size, sizeof(*table));
    used = 0;
    return table != nullptr;
}

void hdestroy() {
    free(table);
    table = nullptr;
    size = used = 0;
}

static size_t hash(const char *s) {
    size_t h = 5381;
    while (*s) {
        h = h * 33 + (unsigned char)*s++;
    }
    return h;
}

ENTRY *hsearch(ENTRY item, ACTION action) {
    if (!table) {
        errno = ESRCH;
        return nullptr;
    }
    for (size_t i = hash(item.key) & (size - 1), n = 0; n < size;
         i = (i + 1) & (size - 1), n++) {
        if (!table[i].key) {
            if (action == FIND || used + 1 >= size) {
                errno = action == FIND ? ESRCH : ENOMEM;
                return nullptr;
            }
            table[i] = item;
            used++;
            return &table[i];
        }
        if (!strcmp(table[i].key, item.key)) {
            return &table[i];
        }
    }
    errno = action == FIND ? ESRCH : ENOMEM;
    return nullptr;
}

/*
 * queue
 */

struct qelem {
    struct qelem *next, *prev;
};

void insque(void *element, void *pred) {
    struct qelem *e = element, *p = pred;
    if (!p) {
        e->next = e->prev = nullptr;
        return;
    }
    e->next = p->next;
    e->prev = p;
    p->next = e;
    if (e->next) {
        e->next->prev = e;
    }
}

void remque(void *element) {
    struct qelem *e = element;
    if (e->prev) {
        e->prev->next = e->next;
    }
    if (e->next) {
        e->next->prev = e->prev;
    }
}

/*
 * linear
 */

void *lfind(const void *key, const void *base, size_t *nelp, size_t width,
            int (*compar)(const void *, const void *)) {
    const char *p = base;
    for (size_t i = 0; i < *nelp; i++, p += width) {
        if (!compar(key, p)) {
            return (void *)p;
        }
    }
    return nullptr;
}

void *lsearch(const void *key, void *base, size_t *nelp, size_t width,
              int (*compar)(const void *, const void *)) {
    void *hit = lfind(key, base, nelp, width, compar);
    if (hit) {
        return hit;
    }
    char *end = (char *)base + *nelp * width;
    memcpy(end, key, width);
    ++*nelp;
    return end;
}

/*
 * binary tree
 */

struct tnode {
    const void *key;
    struct tnode *left, *right;
};

void *tsearch(const void *key, void **rootp,
              int (*compar)(const void *, const void *)) {
    if (!rootp) {
        return nullptr;
    }
    struct tnode **pp = (struct tnode **)rootp;
    while (*pp) {
        int c = compar(key, (*pp)->key);
        if (!c) {
            return *pp;
        }
        pp = c < 0 ? &(*pp)->left : &(*pp)->right;
    }
    struct tnode *n = malloc(sizeof(*n));
    if (!n) {
        return nullptr;
    }
    *n = (struct tnode){key, nullptr, nullptr};
    *pp = n;
    return n;
}

void *tfind(const void *key, void *const *rootp,
            int (*compar)(const void *, const void *)) {
    if (!rootp) {
        return nullptr;
    }
    struct tnode *n = *(struct tnode *const *)rootp;
    while (n) {
        int c = compar(key, n->key);
        if (!c) {
            return n;
        }
        n = c < 0 ? n->left : n->right;
    }
    return nullptr;
}

void *tdelete(const void *__restrict key, void **__restrict rootp,
              int (*compar)(const void *, const void *)) {
    if (!rootp || !*rootp) {
        return nullptr;
    }
    struct tnode **pp = (struct tnode **)rootp;
    struct tnode *parent = nullptr;
    while (*pp) {
        int c = compar(key, (*pp)->key);
        if (!c) {
            break;
        }
        parent = *pp;
        pp = c < 0 ? &(*pp)->left : &(*pp)->right;
    }
    struct tnode *n = *pp;
    if (!n) {
        return nullptr;
    }
    if (!n->left) {
        *pp = n->right;
    } else if (!n->right) {
        *pp = n->left;
    } else {
        struct tnode **sp = &n->right;
        while ((*sp)->left) {
            sp = &(*sp)->left;
        }
        struct tnode *s = *sp;
        *sp = s->right;
        s->left = n->left;
        s->right = n->right;
        *pp = s;
    }
    free(n);
    return parent ? (void *)parent : (void *)rootp;
}

static void walk(const struct tnode *n,
                 void (*action)(const void *, VISIT, int), int depth) {
    if (!n->left && !n->right) {
        action(n, leaf, depth);
        return;
    }
    action(n, preorder, depth);
    if (n->left) {
        walk(n->left, action, depth + 1);
    }
    action(n, postorder, depth);
    if (n->right) {
        walk(n->right, action, depth + 1);
    }
    action(n, endorder, depth);
}

void twalk(const void *root, void (*action)(const void *, VISIT, int)) {
    if (root && action) {
        walk(root, action, 0);
    }
}
