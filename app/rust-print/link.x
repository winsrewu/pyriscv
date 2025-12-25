INCLUDE memory.x

ENTRY(_start)

SECTIONS {
  .text :
  {
    KEEP(*(.init));
    *(.text .text.*);
  } > RAM

  .rodata : { *(.rodata .rodata.*) } > RAM
  .data : { *(.data .data.*) } > RAM
  .bss (NOLOAD) : { *(.bss .bss.* COMMON) } > RAM
}