#pragma once

#include "efidef.h"

#include "efibs.h"
#include "eficon.h"
#include "efidevp.h"
#include "efirs.h"

#include "efierr.h"
#include "efifs.h"
#include "efigop.h"

// EFI_SYSTEM_TABLE
typedef struct {
    EFI_TABLE_HEADER Hdr;

    CHAR16 *FirmwareVendor;
    UINT32  FirmwareRevision;

    EFI_HANDLE                      ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;

    EFI_HANDLE                       ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;

    EFI_HANDLE                       StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;

    EFI_RUNTIME_SERVICES *RuntimeServices;
    EFI_BOOT_SERVICES    *BootServices;

    UINTN                    NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE *ConfigurationTable;
} EFI_SYSTEM_TABLE;

#define EFI_LOADED_IMAGE_PROTOCOL_GUID                                         \
    {0x5B1B31A1,                                                               \
     0x9562,                                                                   \
     0x11d2,                                                                   \
     {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}}

// EFI_IMAGE_UNLOAD
typedef EFI_STATUS (*EFI_IMAGE_UNLOAD)(IN EFI_HANDLE ImageHandle);

// EFI_LOADED_IMAGE_PROTOCOL
typedef struct {
    UINT32            Revision;
    EFI_HANDLE        ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;

    EFI_HANDLE       DeviceHandle;
    EFI_DEVICE_PATH *FilePath;
    VOID            *Reserved;

    UINT32 LoadOptionsSize;
    VOID  *LoadOptions;

    VOID            *ImageBase;
    UINT64           ImageSize;
    EFI_MEMORY_TYPE  ImageCodeType;
    EFI_MEMORY_TYPE  ImageDataType;
    EFI_IMAGE_UNLOAD Unload;
} EFI_LOADED_IMAGE_PROTOCOL;
