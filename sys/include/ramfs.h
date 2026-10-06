#pragma once

#include <stddef.h>

struct fs;

struct fs *ramfs_create(const void *archive, size_t size);
