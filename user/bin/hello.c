#include "../lib/ulib.h"

int main(int argc, char **argv, char **envp) {
    puts("hello: argc ");
    putnum(argc);
    puts("\n");
    for (int i = 0; i < argc; i++) {
        puts("  argv[");
        putnum(i);
        puts("] = ");
        puts(argv[i]);
        puts("\n");
    }
    for (int i = 0; envp[i]; i++) {
        puts("  envp[");
        putnum(i);
        puts("] = ");
        puts(envp[i]);
        puts("\n");
    }
    puts("  page size ");
    putnum((long)getauxval(AT_PAGESZ));
    puts(", entry at ");
    putnum((long)getauxval(AT_ENTRY));
    puts("\n");
    return argc;
}
