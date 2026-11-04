int main(int, char **, char **);

__attribute__((naked, noreturn)) void _start() {
    __asm__ __volatile__("xor ebp, ebp\n\t"
                         "mov rsi, rsp\n\t"
                         "lea rdi, [rip + main]\n\t"
                         "and rsp, -16\n\t"
                         "call __libc_start_main\n\t"
                         "hlt");
}
