#ifndef EFIAPI_H
#define EFIAPI_H

#include "efibind.h"
#include "efidef.h"

#include "eficon.h"

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    EFI_TABLE_HEADER Hdr;

    CHAR16 *FirmwareVendor;
    UINT32  FirmwareRevision;

    // Dummy for ConsoleInHandle and ConIn
    uint8_t dummy[20];

    EFI_HANDLE                       ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;

    EFI_HANDLE                       StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
} EFI_SYSTEM_TABLE;

#endif
