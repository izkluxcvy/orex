#include <stdint.h>

void kern_main(uint64_t framebuffer_addr, uint64_t framebuffer_size) {
    uint8_t *frame_buffer = (uint8_t *)framebuffer_addr;
    for (uint64_t i = 0; i < framebuffer_size; i++) {
        frame_buffer[i] = i % 255; // Set all pixels to white
    }
    while (1) {
        __asm__ __volatile__("hlt");
    }
}
