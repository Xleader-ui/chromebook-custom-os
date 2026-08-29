; boot.asm -- Multiboot header + entry point.
; GRUB looks for this header near the start of the kernel binary and uses
; it to load the kernel and jump to _start in 32-bit protected mode.
;
; flags bit 2 (0x4) = "video mode" request. This tells GRUB/the firmware
; to set up a linear graphics framebuffer and hand us the details, since
; plain VGA text mode (0xB8000) doesn't exist under pure UEFI. When this
; bit is set the header must carry 4 extra fields: mode_type, width,
; height, depth -- our preferred mode. GRUB may give us something close
; instead; the kernel reads back the ACTUAL values at runtime.

[BITS 32]

MBALIGN     equ 0
MEMINFO     equ 0
VIDEOMODE   equ 1 << 2
FLAGS       equ MBALIGN | MEMINFO | VIDEOMODE
MAGIC       equ 0x1BADB002
CHECKSUM    equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    ; These 5 fields are only semantically meaningful if the AOUT_KLUDGE
    ; flag (bit 16) is set -- but GRUB's parser reads the header as a
    ; fixed-offset struct regardless, so they must physically be here
    ; (as zero) or every field after them reads from the wrong offset.
    dd 0            ; header_addr
    dd 0            ; load_addr
    dd 0            ; load_end_addr
    dd 0            ; bss_end_addr
    dd 0            ; entry_addr
    dd 0            ; mode_type: 0 = linear graphics framebuffer (not text)
    dd 1024         ; preferred width
    dd 768          ; preferred height
    dd 32           ; preferred depth (bits per pixel)

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

    ; GRUB leaves: eax = multiboot magic, ebx = pointer to multiboot info
    ; struct (which contains the actual framebuffer address/pitch/etc GRUB
    ; set up). Pass both to kernel_main(uint32_t magic, uint32_t mbi_ptr).
    push ebx
    push eax
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
