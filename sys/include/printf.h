#pragma once

#include <stdarg.h>
#include <stddef.h>

#define PRINTF_MAX_SINKS 4

void printf_add_sink(void (*putc)(char c));

void printf(const char *fmt, ...);
void vprintf(const char *fmt, va_list args);
void printf_write(const char *s, size_t n);
