#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libc.h>
#include <tcb.h>

#ifdef SHARED
__attribute__((visibility("hidden"))) void __dl_run_init();
#endif

char **environ;
char *__progname_full;
char *__progname;
unsigned long *__auxv;

extern void (*const __preinit_array_start[])(int, char **, char **)
    __attribute__((weak, visibility("hidden")));
extern void (*const __preinit_array_end[])(int, char **, char **)
    __attribute__((weak, visibility("hidden")));
extern void (*const __init_array_start[])(int, char **, char **)
    __attribute__((weak, visibility("hidden")));
extern void (*const __init_array_end[])(int, char **, char **)
    __attribute__((weak, visibility("hidden")));

__attribute__((no_stack_protector, noreturn)) void
__libc_start_main(int (*main)(int, char **, char **), long *sp) {
    int argc = (int)sp[0];
    char **argv = (char **)(sp + 1);
    char **envp = argv + argc + 1;
    char **p = envp;
    while (*p) {
        p++;
    }
    environ = envp;
    __progname_full = argc > 0 ? argv[0] : "";
    __progname = strrchr(__progname_full, '/')
                     ? strrchr(__progname_full, '/') + 1
                     : __progname_full;
    __auxv = (unsigned long *)(p + 1);
    __init_tls();
#ifdef SHARED
    __dl_run_init();
#else
    for (size_t i = 0;
         i < (size_t)(__preinit_array_end - __preinit_array_start); i++) {
        __preinit_array_start[i](argc, argv, envp);
    }
    for (size_t i = 0; i < (size_t)(__init_array_end - __init_array_start);
         i++) {
        __init_array_start[i](argc, argv, envp);
    }
#endif
    exit(main(argc, argv, envp));
}
