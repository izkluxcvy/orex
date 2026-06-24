#pragma once

#include "efidef.h"

// EFI_DEVICE_PATH
typedef struct {
    UINT8 Type;
    UINT8 SubType;
    UINT8 Length[2];
} EFI_DEVICE_PATH;
