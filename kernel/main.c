/* SPDX-License-Identifier: Zlib */
#include <iserix/assert.h>
#include <iserix/initcall.h>
#include <iserix/limine.h>
#include <iserix/panic.h>
#include <iserix/printf.h>
#include <iserix/pmm.h>
#include <iserix/utsname.h>

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

    for(i = 0; initcalls[i]; initcalls[i++]());

    panic("main: nothing else to do");
    UNREACHABLE();
}
