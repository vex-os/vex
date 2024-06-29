/* SPDX-License-Identifier: Zlib */
#include <arch/setup.h>
#include <kern/assert.h>
#include <kern/fbcon.h>
#include <kern/panic.h>
#include <kern/printf.h>
#include <kern/version.h>
#include <mm/hhdm.h>
#include <mm/kbase.h>
#include <mm/memmap.h>
#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/vmm.h>

void __noreturn __used kmain(void)
{
    kprintf(KP_INFORM, "%s %s %s", sysname, release, version);

    init_hhdm();
    init_kbase();
    init_memmap();

    setup_arch();

    init_pmm();
    init_slab();
    init_vmm();

    init_fbcon();

    panic("main: nothing else to do");
    unreachable();
}
