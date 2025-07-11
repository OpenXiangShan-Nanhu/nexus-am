OUTPUT_ARCH(riscv)
ENTRY(start)

SECTIONS {
  . = 0x1000000;
  .text : {
    *(.text.startup)
    *(.text)
  }
  .data : { *(.data) }
  .bss : { *(.bss) }
  /DISCARD/ : { *(.eh_frame) }
}