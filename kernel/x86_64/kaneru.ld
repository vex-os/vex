





PHDRS
{
    null PT_NULL FLAGS( 0 );
    text PT_LOAD FLAGS( (1 << 0) | (1 << 2) );
    rodata PT_LOAD FLAGS( (1 << 2) );
    data PT_LOAD FLAGS( (1 << 1) | (1 << 2) );
}

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    .text ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(.text)
        *(.text.*)
    } :text

    .rodata ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(.rodata)
        *(.rodata.*)
    } :rodata

    .data ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(.data)
        *(.data.*)
    } :data

    .bss ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    } :data
}
