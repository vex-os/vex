/* SPDX-License-Identifier: Zlib */
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(kmain)

#define FLAG_E (1 << 0)
#define FLAG_W (1 << 1)
#define FLAG_R (1 << 2)

PHDRS
{
    null    PT_NULL     FLAGS(  0               );
    text    PT_LOAD     FLAGS(  FLAG_E | FLAG_R );
    rodata  PT_LOAD     FLAGS(  FLAG_R          );
    data    PT_LOAD     FLAGS(  FLAG_W | FLAG_R );
    dynamic PT_DYNAMIC  FLAGS(  FLAG_W | FLAG_R );
}

SECTIONS
{
    /* Limine specification requires the binary to be
     * loaded in the negative 2 GiB of addres space */
    . = 0xFFFFFFFF80000000;

    text_start = .;

    .text : {
        *(.text)
        *(.text.*)
    } :text
    
    text_end = .;

    /* Align up to the next page */
    . = ALIGN(CONSTANT(MAXPAGESIZE));

    rodata_start = .;

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } :rodata

    rodata_end = .;

    /* Align up to the next page */
    . = ALIGN(CONSTANT(MAXPAGESIZE));

    data_start = .;

    .data : {
        *(.data)
        *(.data.*)
    } :data

    .dynamic : {
        *(.dynamic)
        *(.dynamic.*)
    } :data :dynamic

    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } :data

    data_end = .;

    /DISCARD/ : {
        *(.discard)
        *(.discard.*)
        *(.initcalls)
        *(.initcalls.*)
        *(.eh_frame*)
        *(.note)
        *(.note.*)
        *(.interp)
    }
}
