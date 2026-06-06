#include <core/panic.h>
#include <init/initcall.h>
#include <mm/memmap.h>

const struct limine_memmap_response* memmap;

static volatile __used __section(".limine_requests") struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
};

define_initcall(init_memmap, INITCALL_STAGE_BSP_BOOTINFO, INITCALL_ORDER_FIRST)
{
    if(memmap_request.response == nullptr || memmap_request.response->entry_count == 0) {
        panic("no valid memory map provided by bootloader");
    }

    memmap = memmap_request.response;
}
