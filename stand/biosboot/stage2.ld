OUTPUT_FORMAT(binary)
ENTRY(_start)
SECTIONS
{
    . = 0x8000;
    .text : {
        *(.text.entry) *(text .text.*)
    }
    .rodata : {
        *(.rodata .rodata.*)
    }
    .data : {
        *(.data .data.*)
    }
    . = ALIGN(16);
    __bss_start = .;
    .bss : {
        *(.bss .bss.* COMMON)
    }
    __bss_end = .;
    /DISCARD/ : {
        *(.comment) *(note*) *(.eh_frame)
    }
}
