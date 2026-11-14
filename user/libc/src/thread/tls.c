#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/auxv.h>

#include <libc.h>
#include <pthread_impl.h>
#include <syscall.h>
#include <tcb.h>

struct phdr {
    uint32_t p_type, p_flags;
    uint64_t p_offset;
    uintptr_t p_vaddr, p_paddr;
    uint64_t p_filesz, p_memsz, p_align;
};

#define PT_TLS       7
#define PROT_RW      3
#define MAP_PRIVANON 0x22
#define ARCH_SET_FS  0x1002

struct tls_module __tls_mods[TLS_MODULES_MAX];
size_t __tls_nmods, __tls_nstatic;
size_t __tls_size, __tls_align = _Alignof(struct pthread);
static volatile int mods_lock;

size_t __tls_area_size() {
    return __tls_size + __tls_align + sizeof(struct pthread);
}

size_t __tls_add_static(const void *image, size_t filesz, size_t memsz,
                        size_t align) {
    align = align ? align : 1;
    if (__tls_nmods == TLS_MODULES_MAX) {
        return 0;
    }
    __tls_size = (__tls_size + memsz + align - 1) & ~(align - 1);
    __tls_mods[__tls_nmods] =
        (struct tls_module){image, filesz, memsz, align, __tls_size};
    __tls_align = align > __tls_align ? align : __tls_align;
    __tls_nstatic = ++__tls_nmods;
    return __tls_nmods;
}

size_t __tls_add_dynamic(const void *image, size_t filesz, size_t memsz,
                         size_t align) {
    __lock(&mods_lock);
    size_t id = 0;
    if (__tls_nmods < TLS_MODULES_MAX) {
        __tls_mods[__tls_nmods] =
            (struct tls_module){image, filesz, memsz, align ? align : 1, 0};
        id = ++__tls_nmods;
    }
    __unlock(&mods_lock);
    return id;
}

__attribute__((no_stack_protector)) struct pthread *__tls_place(void *mem,
                                                                size_t size) {
    uintptr_t top = (uintptr_t)mem + size - sizeof(struct pthread);
    uintptr_t tp = top & ~(__tls_align - 1);
    for (size_t i = 0; i < __tls_nmods; i++) {
        if (__tls_mods[i].filesz) {
            memcpy((char *)tp - __tls_mods[i].offset, __tls_mods[i].image,
                   __tls_mods[i].filesz);
        }
    }
    struct pthread *self = (struct pthread *)tp;
    self->tcb.self = &self->tcb;
    return self;
}

__attribute__((no_stack_protector)) void __init_tls() {
    if (!__tls_nmods) {
        const struct phdr *ph = (const struct phdr *)__getauxval(AT_PHDR);
        for (unsigned long i = 0; ph && i < __getauxval(AT_PHNUM); i++) {
            if (ph[i].p_type == PT_TLS) {
                __tls_add_static((const void *)ph[i].p_vaddr, ph[i].p_filesz,
                                 ph[i].p_memsz, ph[i].p_align);
            }
        }
    }

    size_t size = __tls_area_size();
    long base =
        __syscall6(SYS_mmap, 0, (long)size, PROT_RW, MAP_PRIVANON, -1, 0);
    if ((unsigned long)base > -4096UL) {
        __syscall1(SYS_exit_group, 127);
    }
    struct pthread *self = __tls_place((void *)base, size);
    self->map_base = (void *)base;
    self->map_size = size;

    const uintptr_t *rnd = (const uintptr_t *)__getauxval(AT_RANDOM);
    if (rnd) {
        self->tcb.stack_guard = rnd[0] & ~(uintptr_t)0xff;
        __stack_chk_guard = self->tcb.stack_guard;
        self->tcb.pointer_guard = rnd[1];
    }
    if (__syscall2(SYS_arch_prctl, ARCH_SET_FS, (long)self) != 0) {
        __syscall1(SYS_exit_group, 127);
    }
    self->tid = (int)__syscall1(SYS_set_tid_address, (long)&self->tid);
    __pthread_robust_init(self);
}
