/* linker.ld - Linker script for RISC-V program */

ENTRY(_start)                /* Entry point */

SECTIONS
{
    /* Load everything at physical address 0x80000000 */
    . = 0x80000000;

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }

    /* Discard other sections */
    /DISCARD/ : { *(.comment) *(.eh_frame) }
}