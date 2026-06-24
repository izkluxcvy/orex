#pragma once

#include <stdint.h>

typedef int8_t   INT8;
typedef uint8_t  UINT8;
typedef int16_t  INT16;
typedef uint16_t UINT16;
typedef int32_t  INT32;
typedef uint32_t UINT32;
typedef int64_t  INT64;
typedef uint64_t UINT64;

typedef void VOID;

typedef int64_t  INTN;
typedef uint64_t UINTN;

#define EFIERR(x) (x | 0x8000'0000'0000'0000)

#define EFI_KERNEL_ABI __attribute__((sysv_abi))
