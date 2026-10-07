#pragma once

#include <stddef.h>
#include <stdint.h>

struct vmspace;
struct vnode;

#define EXEC_STRINGS_MAX 0x8000
#define EXEC_ARGV_MAX    256

struct exec_args {
    char  *buf;
    size_t len;
    int    argc;
    int    envc;
};

int  exec_args_from_kernel(struct exec_args *a, const char *const *argv,
                           const char *const *envp);
int  exec_args_from_user(struct exec_args *a, const char *const *uargv,
                         const char *const *uenvp);
void exec_args_free(struct exec_args *a);

struct exec_image {
    uintptr_t entry;
    uintptr_t phdr;
    int       phnum;
    uintptr_t base;
    uintptr_t interp_base;
};

#define EXEC_INTERP_MAX  256
#define EXEC_PIE_BASE    0x0000'5555'5555'4000ULL
#define EXEC_INTERP_BASE 0x0000'7f00'0000'0000ULL

int exec_load(struct vmspace *vm, struct vnode *vn, uintptr_t base,
              struct exec_image *img, char *interp, int main);

int exec_setup_stack(struct vmspace *vm, const struct exec_args *a,
                     const struct exec_image *img, const char *path,
                     uintptr_t *rsp);
