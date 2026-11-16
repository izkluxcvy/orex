#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_off_t
#include <bits/alltypes.h>

#define __need_NULL
#include <stddef.h>
#define __need_va_list
#include <stdarg.h>

typedef struct __FILE FILE;

typedef struct {
    off_t __pos;
} fpos_t;

#define BUFSIZ       8192
#define EOF          (-1)
#define FILENAME_MAX 256
#define FOPEN_MAX    16
#define L_ctermid    20
#define L_tmpnam     20
#define P_tmpdir     "/tmp"
#define TMP_MAX      238328

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#ifdef __cplusplus
}
#endif
