#pragma once

#include "efibind.h"

typedef UINT8  CHAR8;
typedef UINT16 CHAR16;
typedef UINT8  BOOLEAN;

#define TRUE  ((BOOLEAN)1)
#define FALSE ((BOOLEAN)0)

typedef UINTN EFI_STATUS;
typedef UINTN EFI_TPL;
typedef VOID *EFI_HANDLE;
typedef VOID *EFI_EVENT;

#define IN
#define OUT
#define OPTIONAL

// EFI_TABLE_HEADER
typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

// EFI_GUID
typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

// EFI_CONFIGURATION_TABLE
typedef struct {
    EFI_GUID *VendorGuid;
    VOID     *VendorTable;
} EFI_CONFIGURATION_TABLE;

// EFI_TIME
typedef struct {
    UINT16 Year;
    UINT8  Month;
    UINT8  Day;
    UINT8  Hour;
    UINT8  Minute;
    UINT8  Second;
    UINT8  Pad1;
    UINT32 Nanosecond;
    INT16  TimeZone;
    UINT8  Daylight;
    UINT8  Pad2;
} EFI_TIME;

//
// Memory
//

typedef UINT64 EFI_PHYSICAL_ADDRESS;
typedef UINT64 EFI_VIRTUAL_ADDRESS;

// EFI_MEMORY_DESCRIPTOR
typedef struct {
    UINT32               Type;
    UINT32               Pad;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS  VirtualStart;
    UINT64               NumberOfPages;
    UINT64               Attribute;
} EFI_MEMORY_DESCRIPTOR;

// EFI_ALLOCATE_TYPE
typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

// EFI_MEMORY_TYPE
typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;
