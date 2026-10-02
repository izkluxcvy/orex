#pragma once

#include <stdarg.h>

#define PRINTF_MAX_SINKS 4

void printf_add_sink(void (*putc)(char c));

void printf(const char *fmt, ...);
void vprintf(const char *fmt, va_list args);
