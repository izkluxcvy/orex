#include <stddef.h>
#include <stdint.h>

#include <boot_info.h>
#include <paging.h>

#include "../lib/elf.h"
#include "../lib/printf.h"
#include "bios.h"

#define POOL_BASE   0x100000
#define POOL_PAGES  256
#define KERNEL_BASE 0x200000
#define FILE_BUF    0x4000000
#define INITRD_BASE 0x8000000
#define LOW_FREE    0x1000
#define LOW_END     0x7000
#define MAX_MAP     128

static uint8_t drive;
static char    cmdline[256];

static struct boot_info    info;
static struct memmap_entry map[MAX_MAP];
static struct mem_range    ranges[MAX_MAP + 1];
static size_t              nmap, nranges;
static uint64_t            initrd_size;

uintptr_t        kernel_phys_base;
uintptr_t        kernel_virt_base;
size_t           kernel_size;
struct page_pool paging_pool;

static void *memcpy(void *dest, const void *src, size_t n) {
    char       *d = dest;
    const char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

static int memeq(const void *a, const void *b, size_t n) {
    const uint8_t *p = a, *q = b;
    for (size_t i = 0; i < n; i++) {
        if (p[i] != q[i]) {
            return 0;
        }
    }
    return 1;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("out %1, %0" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ __volatile__("in %0, %1" : "=a"(val) : "Nd"(port));
    return val;
}

static int text_screen = 1;

static void putc_both(char c) {
    while (!(inb(0x3f8 + 5) & 0x20)) {
    }
    outb(0x3f8, (uint8_t)c);
    if (text_screen) {
        struct bregs r = {.eax = 0x0e00 | (uint8_t)c, .ebx = 7};
        bios_call(0x10, &r);
    }
}

static void serial_init() {
    outb(0x3f8 + 1, 0x00); // Disable all interrupts
    outb(0x3f8 + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outb(0x3f8 + 0, 0x01); // Set divisor to 1 (lo byte) 115200 baud
    outb(0x3f8 + 1, 0x00); //                  (hi byte)
    outb(0x3f8 + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(0x3f8 + 2, 0xc7); // Enable FIFO, clear them, with 14-byte threshold
    outb(0x3f8 + 4, 0x0b); // IRQs enabled, RTS/DSR set
}

[[noreturn]] static void die(const char *msg) {
    printf("stage2: %s\n", msg);
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

#define E820_BUF 0x7a00

struct e820 {
    uint64_t base, size;
    uint32_t type, attr;
} __attribute__((packed));

static void add_map(uint64_t base, uint64_t size, enum mem_type type) {
    if (size && nmap < MAX_MAP) {
        map[nmap++] =
            (struct memmap_entry){.base = base, .size = size, .type = type};
    }
}

static void add_usable(uint64_t base, uint64_t end) {
    static const uint64_t keep[][2] = {
        {0, LOW_FREE}, {LOW_END, POOL_BASE + POOL_PAGES * 4096ULL}};
    uint64_t kernel_end =
        KERNEL_BASE + ((kernel_size + 0x1fffff) & ~0x1fffffULL);
    uint64_t initrd_end = INITRD_BASE + ((initrd_size + 0xfff) & ~0xfffULL);
    for (uint64_t b = base; b < end;) {
        uint64_t stop = end, skip_to = 0;
        for (int i = 0; i < 4; i++) {
            uint64_t ks =
                i < 2 ? keep[i][0] : (i == 2 ? KERNEL_BASE : INITRD_BASE);
            uint64_t ke =
                i < 2 ? keep[i][1] : (i == 2 ? kernel_end : initrd_end);
            if (ks == ke) {
                continue;
            }
            if (b >= ks && b < ke) {
                skip_to = ke > skip_to ? ke : skip_to;
            } else if (ks > b && ks < stop) {
                stop = ks;
            }
        }
        if (skip_to) {
            add_map(b, (skip_to < end ? skip_to : end) - b, MEM_RESERVED);
            b = skip_to;
            continue;
        }
        add_map(b, stop - b, MEM_USABLE);
        b = stop;
    }
}

static struct e820 e820s[MAX_MAP];
static int         ne820;

static void e820_read() {
    uint32_t next = 0;
    do {
        struct bregs r  = {.eax = 0xe820,
                           .ebx = next,
                           .ecx = 24,
                           .edx = 0x534d4150,
                           .edi = E820_BUF};
        uint32_t     fl = bios_call(0x15, &r);
        if ((fl & 1) || r.eax != 0x534d4150 || ne820 == MAX_MAP) {
            printf("stage2: e820 read failed, fl=%x, eax=%x, ne820=%d\n", fl,
                   r.eax, ne820);
            break;
        }
        memcpy(&e820s[ne820++], (void *)E820_BUF, sizeof(struct e820));
        next = r.ebx;
    } while (next);
    if (!ne820) {
        die("no e820 memory map");
    }
}

static void build_map() {
    for (int i = 0; i < ne820; i++) {
        struct e820 *e = &e820s[i];
        if (e->type == 1) {
            add_usable(e->base, e->base + e->size);
        } else {
            add_map(e->base, e->size, MEM_RESERVED);
        }
        ranges[nranges++] = (struct mem_range){e->base, e->size};
    }
}

static int usable_at(uint64_t base, uint64_t size) {
    for (int i = 0; i < ne820; i++) {
        if (e820s[i].type == 1 && base >= e820s[i].base &&
            base + size <= e820s[i].base + e820s[i].size) {
            return 1;
        }
    }
    return 0;
}

#define DAP      0x7b80
#define DISK_BUF 0x50000
#define SECTOR   512

struct dap {
    uint8_t  size, zero;
    uint16_t count, off, seg;
    uint64_t lba;
} __attribute__((packed));

static void disk_read(uint64_t lba, uint32_t count, void *buf) {
    uint8_t *p = buf;
    while (count) {
        uint32_t n = count > 127 ? 127 : count;
        *(struct dap *)DAP =
            (struct dap){16, 0, (uint16_t)n, 0, DISK_BUF >> 4, lba};
        struct bregs r = {.eax = 0x4200, .edx = drive, .esi = DAP};
        if (bios_call(0x13, &r) & 1) {
            die("disk read failed");
        }
        memcpy(p, (uint8_t *)DISK_BUF, n * SECTOR);
        p += n * SECTOR;
        lba += n;
        count -= n;
    }
}

static const uint8_t esp_type[16] = {0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8,
                                     0xd2, 0x11, 0xba, 0x4b, 0x00, 0xa0,
                                     0xc9, 0x3e, 0xc9, 0x3b};

static uint64_t find_esp() {
    static uint8_t sec[SECTOR];
    disk_read(1, 1, sec);
    if (!memeq(sec, "EFI PART", 8)) {
        die("no GPT on the boot disk");
    }

    uint64_t table = *(uint64_t *)(sec + 72);
    uint32_t count = *(uint32_t *)(sec + 80);
    uint32_t size  = *(uint32_t *)(sec + 84);
    for (uint32_t i = 0; i < count && i < 128; i++) {
        uint64_t off = (uint64_t)i * size;
        disk_read(table + off / SECTOR, 1, sec);
        const uint8_t *e = sec + off % SECTOR;
        if (memeq(e, esp_type, 16)) {
            return *(uint64_t *)(e + 32);
        }
    }
    die("no EFI system partition");
}

// FAT32
static struct {
    uint64_t part, fat, data;
    uint32_t spc, root; // sectors per cluster, root dir first cluster
} fat;

static void fat_init(uint64_t part) {
    static uint8_t bpb[SECTOR];
    disk_read(part, 1, bpb);
    uint16_t bps  = *(uint16_t *)(bpb + 11);
    uint16_t rsvd = *(uint16_t *)(bpb + 14);
    uint32_t fsz  = *(uint32_t *)(bpb + 36);
    if (bps != SECTOR || bpb[510] != 0x55 || bpb[511] != 0xaa || !fsz) {
        die("the EFI system partition is not FAT32");
    }

    fat.part = part;
    fat.spc  = bpb[13];
    fat.fat  = part + rsvd;
    fat.data = fat.fat + (uint64_t)bpb[16] * fsz;
    fat.root = *(uint32_t *)(bpb + 44);
}

static uint32_t fat_next(uint32_t cluster) {
    static uint8_t  sec[SECTOR];
    static uint64_t cached = ~0ULL;
    uint64_t        s      = fat.fat + cluster * 4ULL / SECTOR;
    if (s != cached) {
        disk_read(s, 1, sec);
        cached = s;
    }
    return *(uint32_t *)(sec + cluster * 4 % SECTOR) & 0x0fffffff;
}

static uint64_t fat_read_chain(uint32_t cluster, uint64_t size, uint8_t *buf) {
    static uint8_t last[32 * 1024];
    uint64_t       got   = 0;
    uint64_t       bytes = (uint64_t)fat.spc * SECTOR;
    while (cluster >= 2 && cluster < 0x0ffffff8 && got < size) {
        uint32_t first = cluster;
        uint32_t n     = 1;
        while ((cluster = fat_next(cluster)) == first + n && n < 64 / fat.spc) {
            n++;
        }
        uint64_t run = n * bytes;
        uint64_t lba = fat.data + (uint64_t)(first - 2) * fat.spc;
        if (got + run <= size) {
            disk_read(lba, n * fat.spc, buf + got);
            got += run;
        } else {
            disk_read(lba, n * fat.spc, last);
            memcpy(buf + got, last, size - got);
            got = size;
        }
    }
    return got;
}

static uint64_t fat_file(const char name[11], uint8_t *buf, uint64_t max) {
    static uint8_t dir[64 * 1024];
    uint64_t       n = fat_read_chain(fat.root, sizeof(dir), dir);
    for (uint64_t off = 0; off + 32 <= n && dir[off]; off += 32) {
        const uint8_t *e = dir + off;
        if (e[0] == 0xe5 || (e[11] & 0x0f) == 0x0f || !memeq(e, name, 11)) {
            continue;
        }

        uint32_t cluster =
            (uint32_t)(*(uint16_t *)(e + 20)) << 16 | *(uint16_t *)(e + 26);
        uint32_t size = *(uint32_t *)(e + 28);
        if (size > max) {
            die("file too large");
        }

        return fat_read_chain(cluster, size, buf);
    }
    return 0;
}

#define VBE_INFO  0x7000
#define MODE_INFO 0x7400

static void vbe_init() {
    uint8_t *vi = (uint8_t *)VBE_INFO;
    memcpy(vi, "VBE2", 4);
    struct bregs r = {.eax = 0x4F00, .edi = VBE_INFO};
    bios_call(0x10, &r);
    if ((r.eax & 0xFFFF) != 0x004F || !memeq(vi, "VESA", 4)) {
        printf("stage2: no VBE\n");
        return;
    }
    uint32_t  fp = *(uint32_t *)(vi + 14);
    uint16_t *modes =
        (uint16_t *)(uintptr_t)(((fp >> 16) << 4) + (fp & 0xFFFF));
    int      best = -1, best_score = -1;
    uint16_t list[256];
    int      nmodes = 0;
    for (; *modes != 0xFFFF && nmodes < 256; modes++) {
        list[nmodes++] = *modes;
    }
    for (int i = 0; i < nmodes; i++) {
        struct bregs q = {.eax = 0x4F01, .ecx = list[i], .edi = MODE_INFO};
        bios_call(0x10, &q);
        const uint8_t *m    = (uint8_t *)MODE_INFO;
        uint16_t       attr = *(uint16_t *)m;
        uint16_t       w    = *(uint16_t *)(m + 18);
        uint16_t       h    = *(uint16_t *)(m + 20);
        if ((q.eax & 0xFFFF) != 0x004F || (attr & 0x91) != 0x91 ||
            m[25] != 32 || m[27] != 6) {
            continue; // supported, graphics, linear, 32bpp, direct color
        }
        int score = w == 1024 && h == 768  ? 3
                    : w == 800 && h == 600 ? 2
                    : w <= 1280            ? 1
                                           : 0;
        if (score > best_score) {
            best = i, best_score = score;
        }
    }
    if (best < 0) {
        printf("stage2: no suitable VBE mode\n");
        return;
    }

    struct bregs q = {.eax = 0x4F01, .ecx = list[best], .edi = MODE_INFO};
    bios_call(0x10, &q);
    const uint8_t *m     = (uint8_t *)MODE_INFO;
    uint16_t       w     = *(uint16_t *)(m + 18);
    uint16_t       h     = *(uint16_t *)(m + 20);
    uint16_t       pitch = *(uint16_t *)(m + 16);
    uint32_t       base  = *(uint32_t *)(m + 40);
    printf("stage2: VBE mode %dx%d, pitch=%d, base=%x\n", w, h, pitch, base);
    struct bregs s = {.eax = 0x4F02, .ebx = list[best] | 0x4000u};
    bios_call(0x10, &s);
    if ((s.eax & 0xFFFF) != 0x004F) {
        printf("stage2: failed to set VBE mode\n");
        return;
    }

    text_screen = 0;
    int red_at  = m[32];
    info.fb     = (struct framebuffer){.base                = base,
                                       .size                = (size_t)pitch * h,
                                       .width               = w,
                                       .height              = h,
                                       .pixels_per_scanline = pitch / 4,
                                       .pixel_format        = red_at == 16 ? 1 : 0};
    ranges[nranges++] = (struct mem_range){base, (size_t)pitch * h};
}

static uintptr_t find_rsdp() {
    uintptr_t ebda       = (uintptr_t)(*(uint16_t *)0x40e) << 4;
    uintptr_t where[][2] = {{ebda, ebda + 1024}, {0xe0000, 0x100000}};
    for (int i = 0; i < 2; i++) {
        for (uintptr_t p = where[i][0]; p && p < where[i][1]; p += 16) {
            if (memeq((void *)p, "RSD PTR ", 8)) {
                return p;
            }
        }
    }
    return 0;
}

void stage2_main(uint8_t boot_drive) {
    drive = boot_drive;
    serial_init();
    printf_putc = putc_both;
    printf("stage2: from disk %x\n", drive);

    e820_read();
    fat_init(find_esp());
    uint64_t n =
        fat_file("KERNEL  ELF", (uint8_t *)FILE_BUF, INITRD_BASE - FILE_BUF);
    if (!n) {
        die("failed to load KERNEL.ELF");
    }
    fat_file("CMDLINE TXT", (uint8_t *)cmdline, sizeof(cmdline) - 1);
    for (char *p = cmdline; *p; p++) {
        if (*p == '\r' || *p == '\n') {
            *p = 0;
            break;
        }
    }

    void *entry;
    elf64_scan((void *)FILE_BUF, &entry, &kernel_phys_base, &kernel_virt_base,
               &kernel_size);
    kernel_phys_base = KERNEL_BASE;
    if (!usable_at(KERNEL_BASE, kernel_size) || !usable_at(FILE_BUF, n)) {
        die("not enough memory for kernel");
    }
    elf64_load((void *)FILE_BUF, kernel_phys_base);
    printf("stage2: kernel loaded, %u bytes\n", (unsigned)n);
    uint64_t room = 0;
    for (int i = 0; i < ne820; i++) {
        if (e820s[i].type == 1 && INITRD_BASE >= e820s[i].base &&
            INITRD_BASE < e820s[i].base + e820s[i].size) {
            room = e820s[i].base + e820s[i].size - INITRD_BASE;
        }
    }
    if (room && (initrd_size = fat_file("INITRD  IMG", (uint8_t *)INITRD_BASE,
                                        room)) != 0) {
        printf("stage2: initrd.img loaded, %u bytes\n", (unsigned)initrd_size);
    }

    vbe_init();
    build_map();
    info.memmap           = map;
    info.memmap_count     = nmap;
    info.kernel_phys_base = kernel_phys_base;
    info.kernel_virt_base = kernel_virt_base;
    info.kernel_size      = kernel_size;
    info.acpi_rsdp        = find_rsdp();
    info.initrd_base      = initrd_size ? INITRD_BASE : 0;
    info.initrd_size      = initrd_size;
    memcpy(info.cmdline, cmdline, sizeof(info.cmdline));

    paging_pool = (struct page_pool){
        .base = POOL_BASE, .size = POOL_PAGES * 4096, .used = 0};
    uint64_t pml4 = paging_init(&paging_pool, ranges, nranges, kernel_phys_base,
                                kernel_virt_base, kernel_size);
    if (!pml4) {
        die("paging init failed");
    }
    __asm__ __volatile__("mov cr3, %0" : : "r"(pml4) : "memory");
    ((void (*)(struct boot_info *))entry)(&info);

    die("kernel returned");
}
