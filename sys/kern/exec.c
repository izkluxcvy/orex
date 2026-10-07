#include <stddef.h>
#include <stdint.h>

#include <elf.h>
#include <errno.h>
#include <exec.h>
#include <kmalloc.h>
#include <memcpy.h>
#include <uaccess.h>
#include <vfs.h>
#include <vm.h>

#include "../mm/pmm.h"
#include "cpufunc.h"
#include "pmap.h"

#define EXEC_PHNUM_MAX 64

static int bad_header(const Elf64_Ehdr *eh) {
    return eh->e_ident[0] != ELFMAG0 || eh->e_ident[1] != 'E' ||
           eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F' ||
           eh->e_ident[4] != ELFCLASS64 ||
           (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) ||
           eh->e_machine != EM_X86_64 ||
           eh->e_phentsize != sizeof(Elf64_Phdr) ||
           eh->e_phnum > EXEC_PHNUM_MAX;
}

static int bad_segment(const Elf64_Phdr *ph, uint64_t file_size) {
    return ph->p_offset > file_size ||
           ph->p_filesz > file_size - ph->p_offset ||
           ph->p_filesz > ph->p_memsz || ph->p_vaddr < USER_BASE ||
           ph->p_memsz > USER_TOP - ph->p_vaddr ||
           (ph->p_vaddr - ph->p_offset) % PAGE_SIZE != 0;
}

static int segment_prot(uint32_t flags) {
    return ((flags & PF_R) ? PROT_READ : 0) |
           ((flags & PF_W) ? PROT_WRITE : 0) | ((flags & PF_X) ? PROT_EXEC : 0);
}

int exec_load(struct vmspace *vm, struct vnode *vn, uintptr_t base,
              struct exec_image *img, char *interp, int main) {
    Elf64_Ehdr eh;
    if (vnode_read(vn, &eh, sizeof(eh), 0) != sizeof(eh) || bad_header(&eh)) {
        return -ENOEXEC;
    }
    uintptr_t   bias   = eh.e_type == ET_DYN ? base : 0;
    size_t      phsize = eh.e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr *ph     = kmalloc(phsize ? phsize : 1);
    if (!ph) {
        return -ENOMEM;
    }
    int err =
        vnode_read(vn, ph, phsize, eh.e_phoff) == (long)phsize ? 0 : -ENOEXEC;
    if (interp) {
        interp[0] = '\0';
    }

    uintptr_t brk = 0, phdr = 0, phdr_in_load = 0;
    for (int i = 0; !err && i < eh.e_phnum; i++) {
        if (ph[i].p_type == PT_PHDR) {
            phdr = ph[i].p_vaddr + bias;
        }
        if (ph[i].p_type == PT_INTERP && interp) {
            if (ph[i].p_filesz < 2 || ph[i].p_filesz > EXEC_INTERP_MAX ||
                vnode_read(vn, interp, ph[i].p_filesz, ph[i].p_offset) !=
                    (long)ph[i].p_filesz) {
                err = -ENOEXEC;
            }
            continue;
        }
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) {
            continue;
        }
        Elf64_Phdr seg = ph[i];
        seg.p_vaddr += bias;
        if (bad_segment(&seg, vn->size)) {
            err = -ENOEXEC;
            break;
        }
        uintptr_t start = seg.p_vaddr & ~(PAGE_SIZE - 1);
        uintptr_t end =
            (seg.p_vaddr + seg.p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        err = vm_map(vm, start, end, segment_prot(seg.p_flags), vn,
                     seg.p_offset - (seg.p_vaddr - start),
                     seg.p_vaddr + seg.p_filesz);
        if (err == -EINVAL) {
            err = -ENOEXEC;
        }
        brk = end > brk ? end : brk;
        if (eh.e_phoff >= seg.p_offset &&
            eh.e_phoff + phsize <= seg.p_offset + seg.p_filesz) {
            phdr_in_load = seg.p_vaddr + (eh.e_phoff - seg.p_offset);
        }
    }
    kfree(ph);
    uintptr_t entry = eh.e_entry + bias;
    if (!err && (entry < USER_BASE || entry >= USER_TOP)) {
        err = -ENOEXEC;
    }
    if (!err) {
        if (main) {
            vm->brk_base = vm->brk = brk;
        }
        img->entry = entry;
        img->phdr  = phdr ? phdr : phdr_in_load;
        img->phnum = eh.e_phnum;
        img->base  = bias;
    }
    return err;
}

static int args_init(struct exec_args *a) {
    *a     = (struct exec_args){0};
    a->buf = kmalloc(EXEC_STRINGS_MAX);
    return a->buf ? 0 : -ENOMEM;
}

void exec_args_free(struct exec_args *a) {
    kfree(a->buf);
    a->buf = nullptr;
}

static int args_push(struct exec_args *a, const char *s, int user) {
    size_t room = EXEC_STRINGS_MAX - a->len;
    long   n;

    if (room == 0) {
        return -E2BIG;
    }

    if (user) {
        n = copy_str_from_user(a->buf + a->len, s, room);
        if (n == -ENAMETOOLONG) {
            return -E2BIG;
        }
        if (n < 0) {
            return (int)n;
        }
    } else {
        for (n = 0; s[n]; n++) {
            if ((size_t)n + 1 >= room) {
                return -E2BIG;
            }
            a->buf[a->len + n] = s[n];
        }
        a->buf[a->len + n] = '\0';
    }
    a->len += (size_t)n + 1;
    return 0;
}

static int args_push_vector(struct exec_args *a, const char *const *vec,
                            int user, int *count) {
    for (*count = 0; vec; (*count)++) {
        const char *s;
        if (user) {
            if (copy_from_user(&s, &vec[*count], sizeof(s)) != 0) {
                return -EFAULT;
            }
        } else {
            s = vec[*count];
        }
        if (!s) {
            break;
        }
        if (*count >= EXEC_ARGV_MAX) {
            return -E2BIG;
        }
        int err = args_push(a, s, user);
        if (err) {
            return err;
        }
    }
    return 0;
}

static int args_collect(struct exec_args *a, const char *const *argv,
                        const char *const *envp, int user) {
    int err = args_init(a);
    if (!err) {
        err = args_push_vector(a, argv, user, &a->argc);
    }
    if (!err) {
        err = args_push_vector(a, envp, user, &a->envc);
    }
    if (err) {
        exec_args_free(a);
    }
    return err;
}

int exec_args_from_kernel(struct exec_args *a, const char *const *argv,
                          const char *const *envp) {
    return args_collect(a, argv, envp, 0);
}

int exec_args_from_user(struct exec_args *a, const char *const *uargv,
                        const char *const *uenvp) {
    return args_collect(a, uargv, uenvp, 1);
}

int exec_setup_stack(struct vmspace *vm, const struct exec_args *a,
                     const struct exec_image *img, const char *path,
                     uintptr_t *rsp) {
    uintptr_t top  = vm->stack_top;
    uint64_t  span = USER_STACK_MAX;
    int       err =
        vm_map(vm, top - span, top, PROT_READ | PROT_WRITE, nullptr, 0, 0);
    if (err) {
        return err;
    }

    size_t pathlen = 0;
    while (path[pathlen]) {
        pathlen++;
    }
    uintptr_t execfn  = top - (pathlen + 1);
    uintptr_t strings = (execfn - a->len) & ~(uintptr_t)15;

    uint64_t aux[] = {
        AT_PHDR,   img->phdr,
        AT_PHENT,  sizeof(Elf64_Phdr),
        AT_PHNUM,  (uint64_t)img->phnum,
        AT_PAGESZ, PAGE_SIZE,
        AT_BASE,   img->interp_base,
        AT_FLAGS,  0,
        AT_ENTRY,  img->entry,
        AT_HWCAP,  cpuid(1).d,
        AT_CLKTCK, 100,
        AT_EXECFN, execfn,
        AT_NULL,   0,
    };

    size_t    nptr = 1 + (size_t)a->argc + 1 + (size_t)a->envc + 1;
    size_t    size = nptr * sizeof(uint64_t) + sizeof(aux);
    uintptr_t sp   = (strings - size) & ~(uintptr_t)15;

    uint64_t *vec = kmalloc(size);
    if (!vec) {
        return -ENOMEM;
    }
    size_t    n   = 0;
    uintptr_t str = strings;
    vec[n++]      = (uint64_t)a->argc;
    for (int i = 0; i <= a->argc + a->envc; i++) {
        if (i == a->argc) {
            vec[n++] = 0;
        }
        if (i == a->argc + a->envc) {
            break;
        }
        vec[n++] = str;
        while (a->buf[str - strings]) {
            str++;
        }
        str++;
    }
    vec[n++] = 0;
    memcpy(&vec[n], aux, sizeof(aux));

    err = vm_write(vm, execfn, path, pathlen + 1);
    if (!err) {
        err = vm_write(vm, strings, a->buf, a->len);
    }
    if (!err) {
        err = vm_write(vm, sp, vec, size);
    }
    kfree(vec);

    *rsp = sp;
    return err;
}
