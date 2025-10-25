/* linker.ld - Linker script for RISC-V program */

ENTRY(_start)                /* Entry point */

SECTIONS
{
    /* Load everything at physical address 0x0 */
    . = 0x0;

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