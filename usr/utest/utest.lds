OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
    text   PT_LOAD FLAGS(5); /* R+X */
    rodata PT_LOAD FLAGS(4); /* R   */
    data   PT_LOAD FLAGS(6); /* R+W */
}

SECTIONS
{
    . = 0x10000;

    .text : {
        *(.text)
        *(.text.*)
    } :text

    . = ALIGN(0x1000);

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } :rodata

    . = ALIGN(0x1000);

    .data : {
        *(.data)
        *(.data.*)
    } :data

    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } :data

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note*)
    }
}
