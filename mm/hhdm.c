// SPDX-License-Identifier: BSD-3-Clause

#include <core/panic.h>
#include <init/initfn.h>
#include <lib/limine.h>
#include <mm/hhdm.h>

uintptr_t hhdm_base;

static volatile __used __section(".limine_requests") struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
};

static int init_hhdm(void)
{
    if(hhdm_request.response == nullptr) {
        panic("bootloader did not provide HHDM information");
        unreachable();
    }

    hhdm_base = hhdm_request.response->offset;

    return 0;
}

DEFINE_INITFN(init_hhdm, INITFN_STAGE_BOOTINFO, INITFN_ORDER_FIRST);
