/* linker.ld -- lays out the kernel binary the way GRUB/Multiboot expects. */

ENTRY(_start)

SECTIONS
{
    . = 1M; /* Multiboot-compliant bootloaders load kernels at 1 MiB */

    .text : {
        *(.multiboot)
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(COMMON)
        *(.bss)
    }
}
