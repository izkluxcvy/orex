.intel_syntax noprefix
.code16
.section .text
.global _start
_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti
    mov [drive], dl

    mov ah, 0x41 // Check if extended functions are supported
    mov bx, 0x55aa
    int 0x13
    cmp bx, 0xaa55
    jne fail
    mov cx, [stage2_count]
next:
    test cx, cx
    jz done
    mov ax, cx
    cmp ax, 64
    jbe read_stage2
    mov ax, 64
read_stage2:
    mov [dap_count], ax
    mov si, offset dap
    mov ah, 0x42
    mov dl, [drive]
    push cx
    int 0x13
    pop cx
    jc fail
    mov ax, [dap_count]
    sub cx, ax
    add [dap_lba], ax
    adc word ptr [dap_lba + 2], 0
    shl ax, 5
    add [dap_seg], ax
    jmp next
done:
    mov dl, [drive]
    ljmp 0, 0x8000
fail:
    mov si, offset msg_fail
print_fail_loop:
    lodsb
    test al, al
    jz halt
    mov ah, 0x0e
    mov bx, 7
    int 0x10
    jmp print_fail_loop
halt:
    hlt
    jmp halt

msg_fail: .asciz "Failed to load stage2 bootloader"
drive: .byte 0

.org 0x1a0
dap:
    .byte 16, 0
dap_count:
    .word 0
    .word 0
dap_seg:
    .word 0x800
dap_lba:
    .quad 0
stage2_count:
    .word 0
