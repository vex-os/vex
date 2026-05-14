#include <core/amd64.gdt.h>
#include <core/amd64.idt.h>
#include <core/amd64.picstub.h>

void init_arch(void)
{
    amd64_init_gdt();
    amd64_init_idt();
    amd64_init_picstub();
}
