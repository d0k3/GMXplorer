OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    VECTORS (RX) : ORIGIN = 0x08000000, LENGTH = 64
    CODEMEM (RX) : ORIGIN = 0x08000040, LENGTH = 512K - 64
    BOOTROM (R)  : ORIGIN = 0x08080000, LENGTH = 128K /* BootROM mirrors, don't touch! */
    DATAMEM (RW) : ORIGIN = 0x080A0000, LENGTH = 384K
}

SECTIONS
{
    .vectors : ALIGN(4) {
        __vectors_lma = LOADADDR(.vectors);
        __vectors_vma = ABSOLUTE(.);
        KEEP(*(.vectors));
        . = ALIGN(4);
        __vectors_len = ABSOLUTE(.) - __vectors_vma;
    } >VECTORS AT>CODEMEM

    .text : ALIGN(4) {
        __text_s = ABSOLUTE(.);
        *(.text.start);
        *(.text*);
        . = ALIGN(4);
        __text_e = ABSOLUTE(.);
    } >CODEMEM

    .rodata : ALIGN(4) {
        *(.rodata*);
        . = ALIGN(4);
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
        . = ALIGN(4);
    } >DATAMEM

    .data : ALIGN(4) {
        *(.data*);
        . = ALIGN(4);
    } >DATAMEM

    .bss (NOLOAD) : ALIGN(4) {
        __bss_start = .;
        *(.bss*);
        . = ALIGN(4);
        __bss_end = .;
    } >DATAMEM

    __end__ = ABSOLUTE(.);
}
