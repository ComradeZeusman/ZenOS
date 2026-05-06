ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)

SECTIONS
{
    . = 0x100000;       /* Multiboot standard: load at 1 MB */

    /* Multiboot header must be within the first 8 KB of the image */
    .multiboot ALIGN(4) : {
        *(.multiboot)
    }

    .text ALIGN(4096) : {
        *(.text)
    }

    .rodata ALIGN(4096) : {
        *(.rodata)
    }

    .data ALIGN(4096) : {
        *(.data)
    }

    .bss ALIGN(4096) : {
        *(COMMON)
        *(.bss)
    }

    /* Exported to C as &_kernel_end – used by init_memory to avoid
       allocating memory that overlaps the kernel image. */
    _kernel_end = .;
}