/* SPDX-License-Identifier: Zlib */
#include <arch/setup.h>
#include <core/assert.h>
#include <core/fbcon.h>
#include <core/limine.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/version.h>
#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/vmm.h>

void __noreturn __used kmain(void)
{
    kprintf(KP_INFORM, "%s %s %s", v_sysname, v_release, v_version);

    kassert(hhdm_request.response           != NULL);
    kassert(kernel_address_request.response != NULL);
    kassert(paging_mode_request.response    != NULL);
    kassert(memmap_request.response         != NULL);

    hhdm_offset = hhdm_request.response->offset;
    kernel_base_phys = kernel_address_request.response->physical_base;
    kernel_base_virt = kernel_address_request.response->virtual_base;

    kprintf(KP_INFORM, "main: I'm loaded at physical %p", (void *)(kernel_base_phys));
    kprintf(KP_INFORM, "main: I'm loaded at virtual %p", (void *)(kernel_base_virt));

    setup_arch();

    init_pmm();
    init_slab();
    init_vmm();

    init_fbcon();

    panic("main: nothing else to do");
    unreachable();
}
