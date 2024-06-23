/* SPDX-License-Identifier: Zlib */
#include <vex/assert.h>
#include <vex/initcall.h>
#include <vex/limine.h>
#include <vex/panic.h>
#include <vex/printf.h>
#include <vex/pmm.h>
#include <vex/utsname.h>

#include <arch/pmio.h>

void NORETURN USED kmain(void)
{
    size_t i;

    kprintf(KP_INFORM, "%s %s %s", sysname, release, version);

    kassert(hhdm_request.response           != NULL);
    kassert(kernel_address_request.response != NULL);
    kassert(paging_mode_request.response    != NULL);
    kassert(memmap_request.response         != NULL);

    hhdm_offset = hhdm_request.response->offset;
    kernel_base_phys = kernel_address_request.response->physical_base;
    kernel_base_virt = kernel_address_request.response->virtual_base;

    kprintf(KP_INFORM, "main: I'm loaded at physical %p", (void *)(kernel_base_phys));
    kprintf(KP_INFORM, "main: I'm loaded at virtual %p", (void *)(kernel_base_virt));

    for(i = 0; initcalls[i]; initcalls[i++]());

    panic("main: nothing else to do");
    UNREACHABLE();
}
