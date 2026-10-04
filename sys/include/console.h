#pragma once

#include <boot_info.h>

void console_init(const struct framebuffer *fb);
void console_putc(char c);
