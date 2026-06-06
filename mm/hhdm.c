#include <core/panic.h>
#include <init/initcall.h>
#include <limine.h>
#include <mm/hhdm.h>

uintptr_t hhdm_base;

static volatile __used __section(".limine_requests") struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
};

define_initcall(hhdm, INITCALL_STAGE_BSP_BOOTINFO, INITCALL_ORDER_FIRST)
{
    if(hhdm_request.response == nullptr) {
        panic("bootloader did not provide HHDM information");
    }

    hhdm_base = hhdm_request.response->offset;
}
