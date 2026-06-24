#pragma once

#include "efidef.h"

//
// Time Services
//

// EFI_TIME_CAPABILITIES
typedef struct {
    UINT32  Resolution;
    UINT32  Accuracy;
    BOOLEAN SetsToZero;
} EFI_TIME_CAPABILITIES;

// EFI_GET_TIME
typedef EFI_STATUS (*EFI_GET_TIME)(
    OUT EFI_TIME *Time, OUT EFI_TIME_CAPABILITIES *Capabilities OPTIONAL);

// EFI_SET_TIME
typedef EFI_STATUS (*EFI_SET_TIME)(IN EFI_TIME *Time);

// EFI_GET_WAKEUP_TIME
typedef EFI_STATUS (*EFI_GET_WAKEUP_TIME)(OUT BOOLEAN  *Enabled,
                                          OUT BOOLEAN  *Pending,
                                          OUT EFI_TIME *Time);

// EFI_SET_WAKEUP_TIME
typedef EFI_STATUS (*EFI_SET_WAKEUP_TIME)(IN BOOLEAN        Enable,
                                          IN EFI_TIME *Time OPTIONAL);

//
// Virtual Memory Services
//

// EFI_SET_VIRTUAL_ADDRESS_MAP
typedef EFI_STATUS (*EFI_SET_VIRTUAL_ADDRESS_MAP)(
    IN UINTN MemoryMapSize, IN UINTN DescriptorSize,
    IN UINT32 DescriptorVersion, IN EFI_MEMORY_DESCRIPTOR *VirtualMap);

// EFI_CONVERT_POINTER
typedef EFI_STATUS (*EFI_CONVERT_POINTER)(IN UINTN  DebugDisposition,
                                          IN VOID **Address);

//
// Variable Services
//

// EFI_GET_VARIABLE
typedef EFI_STATUS (*EFI_GET_VARIABLE)(IN CHAR16             *VariableName,
                                       IN EFI_GUID           *VendorGuid,
                                       OUT UINT32 *Attributes OPTIONAL,
                                       IN OUT UINTN *DataSize, OUT VOID *Data);

// EFI_GET_NEXT_VARIABLE_NAME
typedef EFI_STATUS (*EFI_GET_NEXT_VARIABLE_NAME)(IN OUT UINTN *VariableNameSize,
                                                 IN OUT CHAR16 *VariableName,
                                                 OUT EFI_GUID  *VendorGuid);

// EFI_SET_VARIABLE
typedef EFI_STATUS (*EFI_SET_VARIABLE)(IN CHAR16   *VariableName,
                                       IN EFI_GUID *VendorGuid,
                                       IN UINT32 Attributes, IN UINTN DataSize,
                                       IN VOID *Data);

//
// Miscellaneous Services
//

// EFI_GET_NEXT_HIGH_MONO_COUNT
typedef EFI_STATUS (*EFI_GET_NEXT_HIGH_MONO_COUNT)(OUT UINT32 *HighCount);

// EFI_RESET_TYPE
typedef enum {
    EfiResetCold,
    EfiResetWarm,
    EfiResetShutdown,
} EFI_RESET_TYPE;

// EFI_RESET_SYSTEM
typedef EFI_STATUS (*EFI_RESET_SYSTEM)(IN EFI_RESET_TYPE  ResetType,
                                       IN EFI_STATUS      ResetStatus,
                                       IN UINTN           DataSize,
                                       IN VOID *ResetData OPTIONAL);

//
// EFI_RUNTIME_SERVICES
//

typedef struct {
    EFI_TABLE_HEADER Hdr;

    // Time Services
    EFI_GET_TIME        GetTime;
    EFI_SET_TIME        SetTime;
    EFI_GET_WAKEUP_TIME GetWakeupTime;
    EFI_SET_WAKEUP_TIME SetWakeupTime;

    // Virtual Memory Services
    EFI_SET_VIRTUAL_ADDRESS_MAP SetVirtualAddressMap;
    EFI_CONVERT_POINTER         ConvertPointer;

    // Variable Services
    EFI_GET_VARIABLE           GetVariable;
    EFI_GET_NEXT_VARIABLE_NAME GetNextVariableName;
    EFI_SET_VARIABLE           SetVariable;

    // Miscellaneous Services
    EFI_GET_NEXT_HIGH_MONO_COUNT GetNextHighMonotonicCount;
    EFI_RESET_SYSTEM             ResetSystem;
} EFI_RUNTIME_SERVICES;
