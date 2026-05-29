// SPDX-License-Identifier: BSD-3-Clause

#include <core/panic.h>
#include <init/initfn.h>
#include <mm/memmap.h>

const struct limine_memmap_response* memmap;

static volatile __used __section(".limine_requests") struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
};

static int init_memmap(void)
{
    if(memmap_request.response == nullptr || memmap_request.response->entry_count == 0) {
        panic("no valid memory map provided by bootloader");
        unreachable();
    }

    memmap = memmap_request.response;

    return 0;
}

DEFINE_INITFN_BSP(init_memmap, INITFN_STAGE_BSP_BOOTINFO, INITFN_ORDER_FIRST);
