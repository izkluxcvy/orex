.intel_syntax noprefix

#define PML4     0x70000 // the early tables: PML4, PDPT, then 4 PDs
#define RM_STACK 0x7000  // real mode stack, below the MBR page
#define REGS     0x7b00  // struct bregs
#define LM_STACK 0x6f000 // long mode stack, below the tables

.code16
.section .text.entry
.global _start
_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, RM_STACK
    mov [boot_drive], dl

    // enable A20 line
    in al, 0x92
    or al, 2
    and al, 0xfe
    out 0x92, al

    // enable 32-bit protected mode
    lgdt [gdtr]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    ljmp 0x18, offset pm32_start

.code32
pm32_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, LM_STACK

    // PML4[0] -> PDPT, PDPT[0..3] -> 4 PDs of 2 MiB pages: 0 to 4 GiB
    mov edi, PML4
    mov ecx, 6 * 4096 / 4
    xor eax, eax
    rep stosd
    mov dword ptr [PML4], PML4 + 0x1000 + 3
    mov edi, PML4 + 0x1000    // PDPT
    mov eax, PML4 + 0x2000 + 3 // PDs
    mov ecx, 4                // counter
1:
    mov [edi], eax
    add eax, 0x1000
    add edi, 8
    loop 1b
    mov edi, PML4 + 0x2000
    mov eax, 0x83 // 2 MiB page, present, writable
    xor edx, edx
    mov ecx, 2048
2:
    mov [edi], eax
    mov [edi + 4], edx
    add eax, 0x200000
    adc edx, 0
    add edi, 8
    loop 2b

    call long_on
    ljmp 0x08, offset lm_start

long_on:
    mov eax, cr4
    or eax, 0x20 // enable PAE
    mov cr4, eax
    mov eax, PML4
    mov cr3, eax
    mov ecx, 0xc0000080
    rdmsr
    or eax, 0x100
    wrmsr
    mov eax, cr0
    or eax, 0x80000000 // enable long mode
    mov cr0, eax
    ret

.code64
lm_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, LM_STACK
    cld
    lea rdi, [rip + __bss_start]
    lea rcx, [rip + __bss_end]
    sub rcx, rdi
    xor eax, eax
    rep stosb
    movzx edi, byte ptr [boot_drive]
    call stage2_main
3:
    hlt
    jmp 3b

// int bios_call(int n, struct bregs *r)
.global bios_call
bios_call:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov [saved_rsp], rsp
    mov [saved_regs], rsi
    mov byte ptr [int_op + 1], dil
    mov rdi, REGS
    mov rcx, 48 / 8
    rep movsq
    push 0x18
    lea rax, [rip + pm32_down]
    push rax
    retfq

.code32
pm32_down:
    // disable paging and long mode
    mov eax, cr0
    and eax, 0x7fffffff
    mov cr0, eax
    mov ecx, 0xc0000080
    rdmsr
    and eax, 0xfffffeff
    wrmsr
    ljmp 0x20, offset pm16_down

.code16
pm16_down:
    mov ax, 0x28
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov eax, cr0
    and al, 0xfe
    mov cr0, eax
    ljmp 0, offset rm_call

rm_call:
    xor ax, ax
    mov ss, ax
    mov sp, RM_STACK
    mov ds, ax
    mov eax, [REGS + 0]
    mov ebx, [REGS + 4]
    mov ecx, [REGS + 8]
    mov edx, [REGS + 12]
    mov esi, [REGS + 16]
    mov edi, [REGS + 20]
    mov ebp, [REGS + 24]
    push word ptr [REGS + 28] // ds
    push word ptr [REGS + 30] // es
    pop es
    pop ds
    sti
int_op:
    int 0
    cli
    push ds
    push 0
    pop ds
    mov [REGS + 0], eax
    mov [REGS + 4], ebx
    mov [REGS + 8], ecx
    mov [REGS + 12], edx
    mov [REGS + 16], esi
    mov [REGS + 20], edi
    mov [REGS + 24], ebp
    pop word ptr [REGS + 28]
    mov [REGS + 30], es
    pushfd
    pop dword ptr [REGS + 32]

    lgdt [gdtr]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    .byte 0x66, 0xea // ljmp dword 0x18:pm32_up
    .long pm32_up
    .word 0x18

.code32
pm32_up:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    call long_on
    ljmp 0x08, offset lm_up

.code64
lm_up:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, [saved_rsp]
    cld
    mov rdi, [saved_regs]
    mov rsi, REGS
    mov rcx, 48 / 8
    rep movsq
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    mov eax, [REGS + 32]
    ret

.balign 8
gdt:
    .quad 0 // null
    .quad 0x00209a0000000000 // 64-bit code
    .quad 0x00cf92000000ffff // flat data
    .quad 0x00cf9a000000ffff // flat 32-bit code
    .quad 0x00009a000000ffff // 16-bit code
    .quad 0x000092000000ffff // 16-bit data
gdt_end:
gdtr:
    .word gdt_end - gdt - 1
    .long gdt
.global boot_drive
boot_drive:
    .byte 0
.balign 8
saved_rsp:
    .quad 0
saved_regs:
    .quad 0
