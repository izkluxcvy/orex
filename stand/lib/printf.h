#pragma once

extern void (*printf_putc)(char c);
void printf(const char *fmt, ...);
