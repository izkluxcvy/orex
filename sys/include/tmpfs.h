#pragma once

#include <stdint.h>

struct fs;

#define TMPFS_MAX (1ULL << 39)

struct fs *tmpfs_create(uint32_t mode);
