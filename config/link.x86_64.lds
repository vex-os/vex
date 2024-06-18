/* SPDX-License-Identifier: Zlib */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

#define FLAG_E (1 << 0)
#define FLAG_W (1 << 1)
#define FLAG_R (1 << 2)

PHDRS
{
    null    PT_NULL FLAGS(  0               );
    text    PT_LOAD FLAGS(  FLAG_E | FLAG_R );
    rodata  PT_LOAD FLAGS(  FLAG_R          );
    data    PT_LOAD FLAGS(  FLAG_W | FLAG_R );
}

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    .text ALIGN(CONSTANT(MAXPAGESIZE)) : {
        text_start = .;
        *(.text .text.*)
        text_end = .;
    } :text

    .rodata ALIGN(CONSTANT(MAXPAGESIZE)) : {
        rodata_start = .;
        *(.rodata .rodata.*)
        rodata_end = .;
    } :rodata

    .data ALIGN(CONSTANT(MAXPAGESIZE)) : {
        data_start = .;
        *(.data .data.*)
        data_end = .;
    } :data

    .bss ALIGN(CONSTANT(MAXPAGESIZE)) : {
        bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        bss_end = .;
    } :data

    /DISCARD/ : {
        *(.discard .discard.*)
        *(.initcalls .initcalls.*)
        *(.eh_frame*)
        *(.note .note.*)
        *(.interp)
    }
}
