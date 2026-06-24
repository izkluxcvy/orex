void kern_main() {
    while (1) {
        __asm__ __volatile__("hlt");
    }
}
