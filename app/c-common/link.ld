ENTRY(_vectors)

MEMORY
{
	RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 32M
}

__stack_size = 0x8000;

SECTIONS
{
    .vectors : {
		. = ALIGN(4);
		KEEP(*(.vectors))
    } > RAM

    .text : {
      *(.text.entry)
      *(.text)
    } > RAM

    .data : {
        __global_pointer$ = . + 0x800;
        *(.data)
        *(.sdata)
    } > RAM

    .init_array : {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP (*(.init_array .ctors))
        PROVIDE_HIDDEN (__init_array_end = .);
    } > RAM

    .rodata : { *(.rodata) } > RAM
    .bss    : {
        __bss_start = .;
        *(.bss); . = ALIGN(4);
        __bss_end = .;
    } > RAM

    _end = .;
    end = _end;

    /* heap */
    
    _heap_start = .;
    _heap_end = ORIGIN(RAM) + LENGTH(RAM) - __stack_size;

    /* stack */
    .stack ORIGIN(RAM) + LENGTH(RAM) - __stack_size : {
        *(.stack)
    }
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}