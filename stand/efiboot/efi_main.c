#include <efi.h>

#include "../lib/elf.h"

#define KERNEL_FILE_PATH L"\\kernel.elf"

static EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GUID li_guid  = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID sfs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID fi_guid  = EFI_FILE_INFO_ID;

EFI_HANDLE            IH; // Image Handle
EFI_SYSTEM_TABLE     *ST; // System Table
EFI_BOOT_SERVICES    *BS; // Boot Services
EFI_RUNTIME_SERVICES *RS; // Runtime Services

UINTN mapkey;

static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *conout;
static EFI_GRAPHICS_OUTPUT_PROTOCOL    *gop;
static void                            *kernel_entry_point;

static void init_console();
static void init_gop();
static void load_kernel_file();
static void exit_boot_services();
static void jump_to_kernel();

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    IH = ImageHandle;
    ST = SystemTable;
    BS = ST->BootServices;
    RS = ST->RuntimeServices;

    init_console();
    init_gop();
    load_kernel_file();
    exit_boot_services();
    jump_to_kernel();

    // won't reach here
    return EFI_SUCCESS;
}

static void init_console() {
    conout = ST->ConOut;
    conout->ClearScreen(conout);
    conout->EnableCursor(conout, TRUE);
    conout->OutputString(conout, L"Console initialized\r\n");
}

static void init_gop() {
    EFI_STATUS status;

    status = BS->LocateProtocol(&gop_guid, nullptr, (VOID **)&gop);
    if (EFI_ERROR(status)) {
        conout->OutputString(conout, L"Failed to locate GOP protocol\r\n");
        return;
    }

    conout->OutputString(conout, L"GOP initialized\r\n");
}

static void load_kernel_file() {
    EFI_STATUS status;

    // Get loaded image protocol
    EFI_LOADED_IMAGE_PROTOCOL *li;
    status = BS->HandleProtocol(IH, &li_guid, (VOID **)&li);
    if (EFI_ERROR(status)) {
        conout->OutputString(conout,
                             L"Failed to get loaded image protocol\r\n");
        return;
    }

    // Get simple file system protocol
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs;
    status = BS->HandleProtocol(li->DeviceHandle, &sfs_guid, (VOID **)&sfs);
    if (EFI_ERROR(status)) {
        conout->OutputString(conout,
                             L"Failed to get simple file system protocol\r\n");
        return;
    }

    // Open root directory
    EFI_FILE_PROTOCOL *root;
    status = sfs->OpenVolume(sfs, &root);
    if (EFI_ERROR(status)) {
        conout->OutputString(conout, L"Failed to open volume\r\n");
        return;
    }

    // Open kernel file
    EFI_FILE_PROTOCOL *kernel_file;
    status =
        root->Open(root, &kernel_file, KERNEL_FILE_PATH, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        conout->OutputString(conout, L"Failed to open kernel file\r\n");
        return;
    }

    // Get file size
    UINT8 file_info_buffer[256];
    UINTN file_info_size = sizeof(file_info_buffer);
    status = kernel_file->GetInfo(kernel_file, &fi_guid, &file_info_size,
                                  file_info_buffer);
    if (EFI_ERROR(status)) {
        conout->OutputString(conout, L"Failed to get kernel file info\r\n");
        return;
    }
    EFI_FILE_INFO *file_info = (EFI_FILE_INFO *)file_info_buffer;
    UINTN          file_size = file_info->FileSize;

    // Allocate memory for kernel file
    VOID *kernel_buffer;
    status = BS->AllocatePool(EfiLoaderData, file_size, &kernel_buffer);
    if (EFI_ERROR(status)) {
        conout->OutputString(conout,
                             L"Failed to allocate memory for kernel\r\n");
        return;
    }

    // Read kernel file into buffer
    status = kernel_file->Read(kernel_file, &file_size, kernel_buffer);
    if (EFI_ERROR(status)) {
        conout->OutputString(conout, L"Failed to read kernel file\r\n");
        return;
    }

    // Load kernel file
    uint64_t kernel_first_addr;
    size_t   kernel_size;
    elf64_load(kernel_buffer, &kernel_entry_point, &kernel_first_addr,
               &kernel_size);

    // Allocate memory for kernel
    UINTN pages = (kernel_size + 0xFFF) / 0x1000;
    status      = BS->AllocatePages(AllocateAddress, EfiLoaderData, pages,
                                    &kernel_first_addr);
    if (EFI_ERROR(status)) {
        conout->OutputString(conout,
                             L"Failed to allocate pages for kernel\r\n");
        return;
    }

    // Load kernel segments
    elf64_load_segment(kernel_buffer);

    // Free kernel buffer
    BS->FreePool(kernel_buffer);

    conout->OutputString(conout, L"Kernel file loaded\r\n");
}

static void get_memory_map() {
    EFI_STATUS status;

    // Get memory map size
    UINTN  mmsize = 0, descsize = 0;
    UINT32 descver;
    status = BS->GetMemoryMap(&mmsize, nullptr, &mapkey, &descsize, &descver);
    if (status != EFI_BUFFER_TOO_SMALL) {
        conout->OutputString(conout, L"Failed to get memory map size\r\n");
        return;
    }
    mmsize += descsize * 2; // Add extra space for new descriptors

    // Allocate memory for memory map
    EFI_MEMORY_DESCRIPTOR *mmap;
    status = BS->AllocatePool(EfiLoaderData, mmsize, (VOID **)&mmap);
    if (EFI_ERROR(status)) {
        conout->OutputString(conout,
                             L"Failed to allocate memory for memory map\r\n");
        return;
    }

    // Get memory map
    status = BS->GetMemoryMap(&mmsize, mmap, &mapkey, &descsize, &descver);
    if (EFI_ERROR(status)) {
        conout->OutputString(conout, L"Failed to get memory map\r\n");
        return;
    }
}

static void exit_boot_services() {
    EFI_STATUS status;

    int retry;
    for (retry = 0; retry < 3; retry++) {
        get_memory_map();

        status = BS->ExitBootServices(IH, mapkey);
        if (!EFI_ERROR(status)) {
            break;
        }
    }

    if (retry == 3) {
        conout->OutputString(conout, L"Failed to exit boot services\r\n");
    }
}

static void jump_to_kernel() {
    typedef void (*kernel_entry_t)() EFI_KERNEL_ABI;
    kernel_entry_t kernel_entry = (kernel_entry_t)kernel_entry_point;
    kernel_entry();
}
