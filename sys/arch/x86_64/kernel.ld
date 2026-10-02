ALIGN_SIZE = 0x1000;

KERNEL_PHYS_BASE = 0x100000;
KERNEL_VIRT_BASE = 0xffffffff81000000;

ENTRY(_start)

SECTIONS
{
    . = KERNEL_VIRT_BASE;

    .text : AT(ADDR(.text) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        *(.text)
    }

    . = ALIGN(ALIGN_SIZE);
    .rodata : AT(ADDR(.rodata) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        *(.rodata)
    }

    . = ALIGN(ALIGN_SIZE);
    .data : AT(ADDR(.data) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        *(.data)
    }

    . = ALIGN(ALIGN_SIZE);
    .bss : AT(ADDR(.bss) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        *(COMMON)
        *(.bss)
    }
}
