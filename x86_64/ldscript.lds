/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2021, Kirill GPRB */
/* Copyright (c), 2023, KanOS Contributors */
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
        start_text = .;
        *(.text)
        *(.text.*)
        end_text = .;
    } :text

    .rodata ALIGN(CONSTANT(MAXPAGESIZE)) : {
        start_rodata = .;
        *(.rodata)
        *(.rodata.*)
        end_rodata = .;
    } :rodata

    .data ALIGN(CONSTANT(MAXPAGESIZE)) : {
        start_data = .;
        *(.data)
        *(.data.*)
        end_data = .;
    } :data

    .bss ALIGN(CONSTANT(MAXPAGESIZE)) : {
        start_bss = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        end_bss = .;
    } :data
}
