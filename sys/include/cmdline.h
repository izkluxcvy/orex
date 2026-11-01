#pragma once

void cmdline_init(const char *line);
const char *cmdline();
int cmdline_get(const char *key, char *out, unsigned size);
