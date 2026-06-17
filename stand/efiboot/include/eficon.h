#ifndef EFICON_H
#define EFICON_H

#include "efidef.h"

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef EFI_STATUS (*EFI_TEXT_STRING)(
    IN struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, IN CHAR16 *String);

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    uint64_t       *dummy;
    EFI_TEXT_STRING OutputString;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

#endif
