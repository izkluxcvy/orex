#include <stdint.h>

#include <boot_info.h>
#include <efi.h>
#include <paging.h>

#include "../lib/elf.h"
#include "../lib/printf.h"
#include "efidef.h"

#define KERNEL_FILE_PATH L"\\kernel.elf"

static EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GUID li_guid  = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID sfs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID fi_guid  = EFI_FILE_INFO_ID;

EFI_HANDLE            IH; // Image Handle
EFI_SYSTEM_TABLE     *ST; // System Table
EFI_BOOT_SERVICES    *BS; // Boot Services
EFI_RUNTIME_SERVICES *RS; // Runtime Services

static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *conout;
static EFI_GRAPHICS_OUTPUT_PROTOCOL    *gop;
static void                            *kernel_entry_point;
static UINTN                            mapkey;
static EFI_MEMORY_DESCRIPTOR           *mmap;
static UINTN                            mmap_size;
static UINTN                            mmap_descsz;
static struct boot_info                *boot_info;

uintptr_t         kernel_phys_base;
uintptr_t         kernel_virt_base;
size_t            kernel_size;
struct mem_range *mem_ranges;
size_t            mem_range_count;
struct page_pool  paging_pool;

static void init_console();
static void init_gop();
static void load_kernel_file();
static void build_boot_info();
static void exit_boot_services();
extern void machdep_init();
static void jump_to_kernel();

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    IH = ImageHandle;
    ST = SystemTable;
    BS = ST->BootServices;
    RS = ST->RuntimeServices;

    init_console();
    init_gop();
    load_kernel_file();
    build_boot_info();
    exit_boot_services();
    machdep_init();
    jump_to_kernel();

    // won't reach here
    return EFI_SUCCESS;
}

void efi_putc(char c) { conout->OutputString(conout, (CHAR16[]){c, 0}); }

static void init_console() {
    conout = ST->ConOut;

    conout->ClearScreen(conout);
    conout->EnableCursor(conout, TRUE);

    printf_putc = efi_putc;

    printf("Console initialized\r\n");
}

static void init_gop() {
    EFI_STATUS status;

    status = BS->LocateProtocol(&gop_guid, nullptr, (VOID **)&gop);
    if (EFI_ERROR(status)) {
        printf("Failed to locate GOP protocol: %d\r\n", status);
        return;
    }

    printf("GOP initialized: %ux%u, PixelFormat: %d\r\n",
           gop->Mode->Info->HorizontalResolution,
           gop->Mode->Info->VerticalResolution, gop->Mode->Info->PixelFormat);
}

static void load_kernel_file() {
    EFI_STATUS status;

    // Get loaded image protocol
    EFI_LOADED_IMAGE_PROTOCOL *li;
    status = BS->HandleProtocol(IH, &li_guid, (VOID **)&li);
    if (EFI_ERROR(status)) {
        printf("Failed to get loaded image protocol: %d\r\n", status);
        return;
    }

    // Get simple file system protocol
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs;
    status = BS->HandleProtocol(li->DeviceHandle, &sfs_guid, (VOID **)&sfs);
    if (EFI_ERROR(status)) {
        printf("Failed to get simple file system protocol: %d\r\n", status);
        return;
    }

    // Open root directory
    EFI_FILE_PROTOCOL *root;
    status = sfs->OpenVolume(sfs, &root);
    if (EFI_ERROR(status)) {
        printf("Failed to open volume: %d\r\n", status);
        return;
    }

    // Open kernel file
    EFI_FILE_PROTOCOL *kernel_file;
    status =
        root->Open(root, &kernel_file, KERNEL_FILE_PATH, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        printf("Failed to open kernel file: %d\r\n", status);
        return;
    }

    // Get file size
    UINT8 file_info_buffer[sizeof(EFI_FILE_INFO) + sizeof(KERNEL_FILE_PATH)];
    UINTN file_info_size = sizeof(file_info_buffer);
    status = kernel_file->GetInfo(kernel_file, &fi_guid, &file_info_size,
                                  file_info_buffer);
    if (EFI_ERROR(status)) {
        printf("Failed to get kernel file info: %d\r\n", status);
        return;
    }
    EFI_FILE_INFO *file_info = (EFI_FILE_INFO *)file_info_buffer;
    UINTN          file_size = file_info->FileSize;

    // Allocate memory for kernel file
    VOID *kernel_buffer;
    status = BS->AllocatePool(EfiLoaderData, file_size, &kernel_buffer);
    if (EFI_ERROR(status)) {
        printf("Failed to allocate memory for kernel: %d\r\n", status);
        return;
    }

    // Read kernel file into buffer
    status = kernel_file->Read(kernel_file, &file_size, kernel_buffer);
    if (EFI_ERROR(status)) {
        printf("Failed to read kernel file: %d\r\n", status);
        return;
    }

    // Scan kernel file
    elf64_scan(kernel_buffer, &kernel_entry_point, &kernel_phys_base,
               &kernel_virt_base, &kernel_size);

    // Allocate memory for kernel
    UINTN pages = (kernel_size + 0xFFF) / 0x1000;
    status      = BS->AllocatePages(AllocateAddress, EfiLoaderData, pages,
                                    &kernel_phys_base);
    if (EFI_ERROR(status)) {
        printf("Failed to allocate pages for kernel: %d\r\n", status);
        return;
    }

    // Load kernel segments
    elf64_load(kernel_buffer);

    // Free kernel buffer
    BS->FreePool(kernel_buffer);

    printf("Kernel file loaded: entry point: 0x%p, size: %u bytes\r\n",
           kernel_entry_point, kernel_size);
}

static void get_memory_map() {
    EFI_STATUS status;

    // Get memory map size
    mmap_size = 0;
    UINT32 descver;
    status =
        BS->GetMemoryMap(&mmap_size, nullptr, &mapkey, &mmap_descsz, &descver);
    if (status != EFI_BUFFER_TOO_SMALL) {
        printf("Failed to get memory map size: %d\r\n", status);
        return;
    }
    mmap_size += mmap_descsz; // Add extra space for new entries

    // Allocate memory for memory map
    status = BS->AllocatePool(EfiLoaderData, mmap_size, (VOID **)&mmap);
    if (EFI_ERROR(status)) {
        printf("Failed to allocate memory for memory map: %d\r\n", status);
        return;
    }

    // Get memory map
    status =
        BS->GetMemoryMap(&mmap_size, mmap, &mapkey, &mmap_descsz, &descver);
    if (EFI_ERROR(status)) {
        printf("Failed to get memory map: %d\r\n", status);
        return;
    }
}

static size_t build_boot_memmap(struct memmap_entry *boot_memmap,
                                size_t               capacity) {
    size_t count = 0;
    for (UINTN i = 0; i < mmap_size / mmap_descsz && count < capacity; i++) {
        EFI_MEMORY_DESCRIPTOR *desc =
            (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)mmap + i * mmap_descsz);
        if (desc->NumberOfPages == 0) {
            continue;
        }

        boot_memmap[count].base = desc->PhysicalStart;
        boot_memmap[count].size = desc->NumberOfPages * 0x1000;
        switch (desc->Type) {
        case EfiConventionalMemory:
        case EfiBootServicesCode:
        case EfiBootServicesData:
            boot_memmap[count].type = MEM_USABLE;
            break;
        default:
            boot_memmap[count].type = MEM_RESERVED;
            break;
        }
        count++;
    }

    return count;
}

static struct mem_range *build_mem_ranges(size_t *count) {
    size_t            capacity = mmap_size / mmap_descsz;
    struct mem_range *ranges;
    BS->AllocatePool(EfiLoaderData, capacity * sizeof(*ranges),
                     (VOID **)&ranges);

    size_t range_count = 0;
    for (UINTN i = 0; i < capacity; i++) {
        EFI_MEMORY_DESCRIPTOR *desc =
            (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)mmap + i * mmap_descsz);
        if (desc->NumberOfPages == 0) {
            continue;
        }

        ranges[range_count].base = desc->PhysicalStart;
        ranges[range_count].size = desc->NumberOfPages * 0x1000;
        range_count++;
    }

    ranges[range_count].base = gop->Mode->FrameBufferBase;
    ranges[range_count].size = gop->Mode->FrameBufferSize;
    range_count++;

    *count = range_count;
    return ranges;
}

static void build_boot_info() {
    BS->AllocatePool(EfiLoaderData, sizeof(*boot_info), (VOID **)&boot_info);

    get_memory_map();
    size_t               map_capacity = mmap_size / mmap_descsz;
    struct memmap_entry *boot_memmap;
    BS->AllocatePool(EfiLoaderData, map_capacity * sizeof(*boot_memmap),
                     (VOID **)&boot_memmap);

    EFI_PHYSICAL_ADDRESS pool_base = 0;
    BS->AllocatePages(AllocateAnyPages, EfiLoaderData, PAGE_POOL_PAGES,
                      &pool_base);
    paging_pool = (struct page_pool){
        .base = pool_base, .size = PAGE_POOL_PAGES * 0x1000, .used = 0};

    get_memory_map();
    size_t memmap_count = build_boot_memmap(boot_memmap, map_capacity);
    mem_ranges          = build_mem_ranges(&mem_range_count);

    boot_info->fb = (struct framebuffer){
        .base                = gop->Mode->FrameBufferBase,
        .size                = gop->Mode->FrameBufferSize,
        .width               = gop->Mode->Info->HorizontalResolution,
        .height              = gop->Mode->Info->VerticalResolution,
        .pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine,
        .pixel_format        = gop->Mode->Info->PixelFormat,
    };
    boot_info->memmap           = boot_memmap;
    boot_info->memmap_count     = memmap_count;
    boot_info->kernel_phys_base = kernel_phys_base;
    boot_info->kernel_virt_base = kernel_virt_base;
    boot_info->kernel_size      = kernel_size;
}

static void exit_boot_services() {
    EFI_STATUS status;

    printf("Exiting boot services..\r\n");
    int retry;
    for (retry = 0; retry < 3; retry++) {
        get_memory_map();

        status = BS->ExitBootServices(IH, mapkey);
        if (!EFI_ERROR(status)) {
            break;
        }
    }

    if (retry == 3) {
        printf("Failed to exit boot services: %d\r\n", status);
    }
}

static void jump_to_kernel() {
    typedef void (*kernel_entry_t)(struct boot_info *) EFI_KERNEL_ABI;
    kernel_entry_t kernel_entry = (kernel_entry_t)kernel_entry_point;
    kernel_entry(boot_info);
}
