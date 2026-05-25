// SPDX-License-Identifier: BSD-3-Clause

#include <core/panic.h>
#include <init/initfn.h>
#include <lib/limine.h>
#include <mm/kbase.h>

uintptr_t kbase_phys;
uintptr_t kbase_virt;

static volatile __used __section(".limine_requests") struct limine_executable_address_request kbase_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0,
};

static int init_kbase(void)
{
    if(kbase_request.response == nullptr) {
        panic("bootloader did not provide executable address information");
        unreachable();
    }

    kbase_phys = kbase_request.response->physical_base;
    kbase_virt = kbase_request.response->virtual_base;

    return 0;
}

DEFINE_INITFN(init_kbase, INITFN_STAGE_BOOTINFO, INITFN_ORDER_FIRST);
