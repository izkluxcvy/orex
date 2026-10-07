ENTRY(_start)

PHDRS
{
    phdr PT_PHDR PHDRS;
    text PT_LOAD FILEHDR PHDRS FLAGS(5); /* R X */
    rodata PT_LOAD FLAGS(4);             /* R */
    data PT_LOAD FLAGS(6);               /* R W */
    tls PT_TLS;
}

SECTIONS
{
    . = 0x400000 + SIZEOF_HEADERS;

    .text : { *(.text*) } :text

    . = ALIGN(0x1000);
    .rodata : { *(.rodata*) } :rodata

    . = ALIGN(0x1000);
    .tdata : { *(.tdata*) } :data :tls
    .tbss : { *(.tbss*) } :data :tls
    .got : { *(.got*) } :data
    .data : { *(.data*) } :data
    .bss : { *(.bss*) } :data
}
