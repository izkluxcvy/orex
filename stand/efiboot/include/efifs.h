#pragma once

#include "efidef.h"

//
// EFI_FILE_PROTOCOL
//

struct _EFI_FILE_PROTOCOL;

#define EFI_FILE_MODE_READ  0x0000000000000001
#define EFI_FILE_MODE_WRITE 0x0000000000000002

// EFI_FILE_OPEN
typedef EFI_STATUS (*EFI_FILE_OPEN)(IN struct _EFI_FILE_PROTOCOL   *This,
                                    OUT struct _EFI_FILE_PROTOCOL **NewHandle,
                                    IN CHAR16 *FileName, IN UINT64 OpenMode,
                                    IN UINT64 Attributes);

// EFI_FILE_CLOSE
typedef EFI_STATUS (*EFI_FILE_CLOSE)(IN struct _EFI_FILE_PROTOCOL *This);

// EFI_FILE_DELETE
typedef EFI_STATUS (*EFI_FILE_DELETE)(IN struct _EFI_FILE_PROTOCOL *This);

// EFI_FILE_READ
typedef EFI_STATUS (*EFI_FILE_READ)(IN struct _EFI_FILE_PROTOCOL *This,
                                    IN OUT UINTN *BufferSize, OUT VOID *Buffer);

// EFI_FILE_WRITE
typedef EFI_STATUS (*EFI_FILE_WRITE)(IN struct _EFI_FILE_PROTOCOL *This,
                                     IN OUT UINTN *BufferSize, IN VOID *Buffer);

// EFI_FILE_GET_POSITION
typedef EFI_STATUS (*EFI_FILE_GET_POSITION)(IN struct _EFI_FILE_PROTOCOL *This,
                                            OUT UINT64 *Position);

// EFI_FILE_SET_POSITION
typedef EFI_STATUS (*EFI_FILE_SET_POSITION)(IN struct _EFI_FILE_PROTOCOL *This,
                                            IN UINT64 Position);

// EFI_FILE_INFO
#define EFI_FILE_INFO_ID                                                       \
    {0x09576e92,                                                               \
     0x6d3f,                                                                   \
     0x11d2,                                                                   \
     {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}
typedef struct {
    UINT64   Size;
    UINT64   FileSize;
    UINT64   PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64   Attribute;
    CHAR16   FileName[];
} EFI_FILE_INFO;

// EFI_FILE_GET_INFO
typedef EFI_STATUS (*EFI_FILE_GET_INFO)(IN struct _EFI_FILE_PROTOCOL *This,
                                        IN EFI_GUID  *InformationType,
                                        IN OUT UINTN *BufferSize,
                                        OUT VOID     *Buffer);

// EFI_FILE_SET_INFO
typedef EFI_STATUS (*EFI_FILE_SET_INFO)(IN struct _EFI_FILE_PROTOCOL *This,
                                        IN EFI_GUID *InformationType,
                                        IN UINTN BufferSize, IN VOID *Buffer);

// EFI_FILE_FLUSH
typedef EFI_STATUS (*EFI_FILE_FLUSH)(IN struct _EFI_FILE_PROTOCOL *This);

// EFI_FILE_PROTOCOL
typedef struct _EFI_FILE_PROTOCOL {
    UINT64                Revision;
    EFI_FILE_OPEN         Open;
    EFI_FILE_CLOSE        Close;
    EFI_FILE_DELETE       Delete;
    EFI_FILE_READ         Read;
    EFI_FILE_WRITE        Write;
    EFI_FILE_GET_POSITION GetPosition;
    EFI_FILE_SET_POSITION SetPosition;
    EFI_FILE_GET_INFO     GetInfo;
    EFI_FILE_SET_INFO     SetInfo;
    EFI_FILE_FLUSH        Flush;
} EFI_FILE_PROTOCOL;

//
// EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
//

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID                                   \
    {0x0964e5b22,                                                              \
     0x6459,                                                                   \
     0x11d2,                                                                   \
     {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

// EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME
typedef EFI_STATUS (*EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME)(
    IN struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
    OUT EFI_FILE_PROTOCOL                     **Root);

// EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64                                      Revision;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME OpenVolume;
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
