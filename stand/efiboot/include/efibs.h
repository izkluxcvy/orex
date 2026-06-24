#pragma once

#include "efidef.h"
#include "efidevp.h"

//
// Task Priority Services
//

// EFI_RAISE_TPL
typedef EFI_TPL (*EFI_RAISE_TPL)(IN EFI_TPL NewTpl);

// EFI_RESTORE_TPL
typedef VOID (*EFI_RESTORE_TPL)(IN EFI_TPL OldTpl);

//
// Memory Allocation Services
//

// EFI_ALLOCATE_PAGES
typedef EFI_STATUS (*EFI_ALLOCATE_PAGES)(IN EFI_ALLOCATE_TYPE      Type,
                                         IN EFI_MEMORY_TYPE        MemoryType,
                                         IN UINTN                  Pages,
                                         OUT EFI_PHYSICAL_ADDRESS *Memory);

// EFI_FREE_PAGES
typedef EFI_STATUS (*EFI_FREE_PAGES)(IN EFI_PHYSICAL_ADDRESS Memory,
                                     IN UINTN                Pages);

// EFI_GET_MEMORY_MAP
typedef EFI_STATUS (*EFI_GET_MEMORY_MAP)(
    IN OUT UINTN *MemoryMapSize, IN OUT EFI_MEMORY_DESCRIPTOR *MemoryMap,
    OUT UINTN *MapKey, OUT UINTN *DescriptorSize,
    OUT UINT32 *DescriptorVersion);

// EFI_ALLOCATE_POOL
typedef EFI_STATUS (*EFI_ALLOCATE_POOL)(IN EFI_MEMORY_TYPE PoolType,
                                        IN UINTN Size, OUT VOID **Buffer);

// EFI_FREE_POOL
typedef EFI_STATUS (*EFI_FREE_POOL)(IN VOID *Buffer);

//
// Event & Timer Services
//

// EFI_EVENT_NOTIFY
typedef VOID (*EFI_EVENT_NOTIFY)(IN EFI_EVENT Event, IN VOID *Context);

// EFI_CREATE_EVENT
typedef EFI_STATUS (*EFI_CREATE_EVENT)(IN UINT32 Type, IN EFI_TPL NotifyTpl,
                                       IN EFI_EVENT_NOTIFY NotifyFunction,
                                       IN VOID            *NotifyContext,
                                       OUT EFI_EVENT      *Event);

// EFI_TIMER_DELAY
typedef enum { TimerCancel, TimerPeriodic, TimerRelative } EFI_TIMER_DELAY;

// EFI_SET_TIMER
typedef EFI_STATUS (*EFI_SET_TIMER)(IN EFI_EVENT Event, IN EFI_TIMER_DELAY Type,
                                    IN UINT64 TriggerTime);

// EFI_WAIT_FOR_EVENT
typedef EFI_STATUS (*EFI_WAIT_FOR_EVENT)(IN UINTN      NumberOfEvents,
                                         IN EFI_EVENT *Event, OUT UINTN *Index);

// EFI_SIGNAL_EVENT
typedef EFI_STATUS (*EFI_SIGNAL_EVENT)(IN EFI_EVENT Event);

// EFI_CLOSE_EVENT
typedef EFI_STATUS (*EFI_CLOSE_EVENT)(IN EFI_EVENT Event);

// EFI_CHECK_EVENT
typedef EFI_STATUS (*EFI_CHECK_EVENT)(IN EFI_EVENT Event);

//
// Protocol Handler Services
//

// EFI_INTERFACE_TYPE
typedef enum { EFI_NATIVE_INTERFACE } EFI_INTERFACE_TYPE;

// EFI_INSTALL_PROTOCOL_INTERFACE
typedef EFI_STATUS (*EFI_INSTALL_PROTOCOL_INTERFACE)(
    IN OUT EFI_HANDLE *Handle, IN EFI_GUID *Protocol,
    IN EFI_INTERFACE_TYPE InterfaceType, IN VOID *Interface);

// EFI_REINSTALL_PROTOCOL_INTERFACE
typedef EFI_STATUS (*EFI_REINSTALL_PROTOCOL_INTERFACE)(IN EFI_HANDLE Handle,
                                                       IN EFI_GUID  *Protocol,
                                                       IN VOID *OldInterface,
                                                       IN VOID *NewInterface);

// EFI_UNINSTALL_PROTOCOL_INTERFACE
typedef EFI_STATUS (*EFI_UNINSTALL_PROTOCOL_INTERFACE)(IN EFI_HANDLE Handle,
                                                       IN EFI_GUID  *Protocol,
                                                       IN VOID      *Interface);

/// EFI_HANDLE_PROTOCOL
typedef EFI_STATUS (*EFI_HANDLE_PROTOCOL)(IN EFI_HANDLE Handle,
                                          IN EFI_GUID  *Protocol,
                                          OUT VOID    **Interface);

// EFI_REGISTER_PROTOCOL_NOTIFY
typedef EFI_STATUS (*EFI_REGISTER_PROTOCOL_NOTIFY)(IN EFI_GUID *Protocol,
                                                   IN EFI_EVENT Event,
                                                   OUT VOID   **Registration);

// EFI_LOCATE_SEARCH_TYPE
typedef enum {
    AllHandles,
    ByRegisterNotify,
    ByProtocol
} EFI_LOCATE_SEARCH_TYPE;

// EFI_LOCATE_HANDLE
typedef EFI_STATUS (*EFI_LOCATE_HANDLE)(IN EFI_LOCATE_SEARCH_TYPE SearchType,
                                        IN EFI_GUID *Protocol     OPTIONAL,
                                        IN VOID *SearchKey        OPTIONAL,
                                        IN OUT UINTN             *BufferSize,
                                        OUT EFI_HANDLE           *Buffer);

// EFI_LOCATE_DEVICE_PATH
typedef EFI_STATUS (*EFI_LOCATE_DEVICE_PATH)(
    IN EFI_GUID *Protocol, IN OUT EFI_DEVICE_PATH **DevicePath,
    OUT EFI_HANDLE *Device);

// EFI_INSTALL_CONFIGURATION_TABLE
typedef EFI_STATUS (*EFI_INSTALL_CONFIGURATION_TABLE)(IN EFI_GUID *Guid,
                                                      IN VOID     *Table);

// EFI_CONNECT_CONTROLLER
typedef EFI_STATUS (*EFI_CONNECT_CONTROLLER)(
    IN EFI_HANDLE ControllerHandle, IN EFI_HANDLE *DriverImageHandle OPTIONAL,
    IN EFI_DEVICE_PATH *RemainingDevicePath OPTIONAL, IN BOOLEAN Recursive);

// EFI_DISCONNECT_CONTROLLER
typedef EFI_STATUS (*EFI_DISCONNECT_CONTROLLER)(
    IN EFI_HANDLE ControllerHandle, IN EFI_HANDLE DriverImageHandle OPTIONAL,
    IN EFI_HANDLE ChildHandle OPTIONAL);

// EFI_OPEN_PROTOCOL
typedef EFI_STATUS (*EFI_OPEN_PROTOCOL)(IN EFI_HANDLE        Handle,
                                        IN EFI_GUID         *Protocol,
                                        OUT VOID **Interface OPTIONAL,
                                        IN EFI_HANDLE        AgentHandle,
                                        IN EFI_HANDLE        ControllerHandle,
                                        IN UINT32            Attributes);

// EFI_CLOSE_PROTOCOL
typedef EFI_STATUS (*EFI_CLOSE_PROTOCOL)(IN EFI_HANDLE Handle,
                                         IN EFI_GUID  *Protocol,
                                         IN EFI_HANDLE AgentHandle,
                                         IN EFI_HANDLE ControllerHandle);

// EFI_OPEN_PROTOCOL_INFORMATION_ENTRY
typedef struct {
    EFI_HANDLE AgentHandle;
    EFI_HANDLE ControllerHandle;
    UINT32     Attributes;
    UINT32     OpenCount;
} EFI_OPEN_PROTOCOL_INFORMATION_ENTRY;

// EFI_OPEN_PROTOCOL_INFORMATION
typedef EFI_STATUS (*EFI_OPEN_PROTOCOL_INFORMATION)(
    IN EFI_HANDLE Handle, IN EFI_GUID *Protocol,
    OUT EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **EntryBuffer,
    OUT UINTN                                *EntryCount);

// EFI_PROTOCOLS_PER_HANDLE
typedef EFI_STATUS (*EFI_PROTOCOLS_PER_HANDLE)(IN EFI_HANDLE   Handle,
                                               OUT EFI_GUID ***ProtocolBuffer,
                                               OUT UINTN *ProtocolBufferCount);

// EFI_LOCATE_HANDLE_BUFFER
typedef EFI_STATUS (*EFI_LOCATE_HANDLE_BUFFER)(
    IN EFI_LOCATE_SEARCH_TYPE SearchType, IN EFI_GUID *Protocol OPTIONAL,
    IN VOID *SearchKey OPTIONAL, OUT UINTN *NoHandles, OUT EFI_HANDLE **Buffer);

// EFI_LOCATE_PROTOCOL
typedef EFI_STATUS (*EFI_LOCATE_PROTOCOL)(IN EFI_GUID           *Protocol,
                                          OUT VOID *Registration OPTIONAL,
                                          OUT VOID             **Interface);

// EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES
typedef EFI_STATUS (*EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES)(
    IN OUT EFI_HANDLE *Handle, ...);

// EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES
typedef EFI_STATUS (*EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES)(
    IN EFI_HANDLE *Handle, ...);

//
// Image Services
//

// EFI_IMAGE_LOAD
typedef EFI_STATUS (*EFI_IMAGE_LOAD)(IN BOOLEAN            BootPolicy,
                                     IN EFI_HANDLE         ParentImageHandle,
                                     IN EFI_DEVICE_PATH   *FilePath,
                                     IN VOID *SourceBuffer OPTIONAL,
                                     IN UINTN SourceSize   OPTIONAL,
                                     OUT EFI_HANDLE       *ImageHandle);

// EFI_IMAGE_START
typedef EFI_STATUS (*EFI_IMAGE_START)(IN EFI_HANDLE         ImageHandle,
                                      OUT UINTN            *ExitDataSize,
                                      OUT CHAR16 **ExitData OPTIONAL);

// EFI_EXIT
typedef EFI_STATUS (*EFI_EXIT)(IN EFI_HANDLE ImageHandle,
                               IN EFI_STATUS ExitStatus, IN UINTN ExitDataSize,
                               IN CHAR16 *ExitData OPTIONAL);

// EFI_IMAGE_UNLOAD
typedef EFI_STATUS (*EFI_IMAGE_UNLOAD)(IN EFI_HANDLE ImageHandle);

/// EFI_EXIT_BOOT_SERVICES
typedef EFI_STATUS (*EFI_EXIT_BOOT_SERVICES)(IN EFI_HANDLE ImageHandle,
                                             IN UINTN      MapKey);

//
// Miscellaneous Services
//

// EFI_GET_NEXT_MONOTONIC_COUNT
typedef EFI_STATUS (*EFI_GET_NEXT_MONOTONIC_COUNT)(OUT UINT64 *Count);

// EFI_STALL
typedef EFI_STATUS (*EFI_STALL)(IN UINTN Microseconds);

// EFI_SET_WATCHDOG_TIMER
typedef EFI_STATUS (*EFI_SET_WATCHDOG_TIMER)(IN UINTN  Timeout,
                                             IN UINT64 WatchdogCode,
                                             IN UINTN  DataSize,
                                             IN CHAR16 *WatchdogData OPTIONAL);

// EFI_CALCULATE_CRC32
typedef EFI_STATUS (*EFI_CALCULATE_CRC32)(IN VOID *Data, IN UINTN DataSize,
                                          OUT UINT32 *Crc32);

// EFI_COPY_MEM
typedef VOID (*EFI_COPY_MEM)(IN VOID *Destination, IN VOID *Source,
                             IN UINTN Length);

// EFI_SET_MEM
typedef VOID (*EFI_SET_MEM)(IN VOID *Buffer, IN UINTN Size, IN UINT8 Value);

//
// EFI_BOOT_SERVICES
//

typedef struct {
    EFI_TABLE_HEADER Hdr;

    // Task Priority Services
    EFI_RAISE_TPL   RaiseTPL;
    EFI_RESTORE_TPL RestoreTPL;

    // Memory Services
    EFI_ALLOCATE_PAGES AllocatePages;
    EFI_FREE_PAGES     FreePages;
    EFI_GET_MEMORY_MAP GetMemoryMap;
    EFI_ALLOCATE_POOL  AllocatePool;
    EFI_FREE_POOL      FreePool;

    // Event & Timer Services
    EFI_CREATE_EVENT   CreateEvent;
    EFI_SET_TIMER      SetTimer;
    EFI_WAIT_FOR_EVENT WaitForEvent;
    EFI_SIGNAL_EVENT   SignalEvent;
    EFI_CLOSE_EVENT    CloseEvent;
    EFI_CHECK_EVENT    CheckEvent;

    // Protocol Handler Services (Part.1)
    EFI_INSTALL_PROTOCOL_INTERFACE   InstallProtocolInterface;
    EFI_REINSTALL_PROTOCOL_INTERFACE ReinstallProtocolInterface;
    EFI_UNINSTALL_PROTOCOL_INTERFACE UninstallProtocolInterface;
    EFI_HANDLE_PROTOCOL              HandleProtocol;
    VOID                            *Reserved;
    EFI_REGISTER_PROTOCOL_NOTIFY     RegisterProtocolNotify;
    EFI_LOCATE_HANDLE                LocateHandle;
    EFI_LOCATE_DEVICE_PATH           LocateDevicePath;
    EFI_INSTALL_CONFIGURATION_TABLE  InstallConfigurationTable;

    // Image Services
    EFI_IMAGE_LOAD         LoadImage;
    EFI_IMAGE_START        StartImage;
    EFI_EXIT               Exit;
    EFI_IMAGE_UNLOAD       UnloadImage;
    EFI_EXIT_BOOT_SERVICES ExitBootServices;

    // Miscellaneous Services (Part.1)
    EFI_GET_NEXT_MONOTONIC_COUNT GetNextMonotonicCount;
    EFI_STALL                    Stall;
    EFI_SET_WATCHDOG_TIMER       SetWatchdogTimer;

    // Protocol Handler Services (Part.2)
    EFI_CONNECT_CONTROLLER                   ConnectController;
    EFI_DISCONNECT_CONTROLLER                DisconnectController;
    EFI_OPEN_PROTOCOL                        OpenProtocol;
    EFI_CLOSE_PROTOCOL                       CloseProtocol;
    EFI_OPEN_PROTOCOL_INFORMATION            OpenProtocolInformation;
    EFI_PROTOCOLS_PER_HANDLE                 ProtocolsPerHandle;
    EFI_LOCATE_HANDLE_BUFFER                 LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL                      LocateProtocol;
    EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES InstallMultipleProtocolInterfaces;
    EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES
    UninstallMultipleProtocolInterfaces;

    // Miscellaneous Services (Part.2)
    EFI_CALCULATE_CRC32 CalculateCrc32;
    EFI_COPY_MEM        CopyMem;
    EFI_SET_MEM         SetMem;
} EFI_BOOT_SERVICES;
