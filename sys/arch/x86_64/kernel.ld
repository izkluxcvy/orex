ALIGN_SIZE = 0x1000;

KERNEL_PHYS_BASE = 0x200000;
KERNEL_VIRT_BASE = 0xffffffff80200000;

ENTRY(_start)

SECTIONS
{
    . = KERNEL_VIRT_BASE;

    .text : AT(ADDR(.text) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        *(.text*)
    }

    . = ALIGN(ALIGN_SIZE);
    _user_text_start = .;
    .user_text : AT(ADDR(.user_text) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        *(.user_text)
    }
    . = ALIGN(ALIGN_SIZE);
    _user_text_end = .;

    .rodata : AT(ADDR(.rodata) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        *(.rodata*)
    }

    . = ALIGN(ALIGN_SIZE);
    .data : AT(ADDR(.data) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        *(.data*)
    }

    . = ALIGN(ALIGN_SIZE);
    .bss : AT(ADDR(.bss) - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE)
    {
        *(COMMON)
        *(.bss*)
    }
}
