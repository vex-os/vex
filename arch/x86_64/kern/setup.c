/* SPDX-License-Identifier: Zlib */
#include <arch/bxcon.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/intreq.h>
#include <arch/setup.h>

void setup_arch(void)
{
    init_bxcon();

    init_gdt();
    init_idt();
    init_intreq();
}
