/* SPDX-License-Identifier: BSD-2-Clause */
#include <arch/8259.h>
#include <arch/bxcon.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/intreq.h>
#include <arch/setup.h>

void init_arch_early(void)
{
    init_bxcon();
}

void init_arch(void)
{
    init_gdt();
    init_idt();
    init_intreq();

    init_8259();
}
