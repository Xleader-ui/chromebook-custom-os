; boot.asm -- Multiboot header + entry point.
; GRUB looks for this header near the start of the kernel binary and uses
; it to load the kernel and jump to _start in 32-bit protected mode.

[BITS 32]

section .multiboot
align 4
    dd 0x1BADB002              ; magic number GRUB looks for
    dd 0x00                    ; flags
    dd -(0x1BADB002 + 0x00)    ; checksum (magic + flags + checksum = 0)

section .bss
align 16
stack_bottom:
    resb 16384                 ; 16 KiB stack
stack_top:

section .text
global _start
extern kernel_main

_start:
    cli                        ; disable interrupts until we set up our own
    mov esp, stack_top         ; set up the stack
    call kernel_main           ; jump into our C kernel

.hang:
    cli
    hlt
    jmp .hang
