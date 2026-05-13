OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(kmain)

#define FLAG_E (1 << 0)
#define FLAG_W (1 << 1)
#define FLAG_R (1 << 2)

PHDRS
{
    text    PT_LOAD FLAGS(  FLAG_E | FLAG_R );
    rodata  PT_LOAD FLAGS(  FLAG_R          );
    data    PT_LOAD FLAGS(  FLAG_W | FLAG_R );
}

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    text_start = .;

    .text : {
        *(.text)
        *(.text.*)
    } :text
    
    text_end = .;

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    rodata_start = .;

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } :rodata

    rodata_end = .;

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    data_start = .;

    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :data

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data : {
        *(.data)
        *(.data.*)
    } :data

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } :data

    data_end = .;

    /DISCARD/ : {
        *(.discard)
        *(.discard.*)
        *(.eh_frame*)
        *(.note.*)
        *(.note)
    }
}
