#include <bus/acpi/rsdp.h>
#include <core/panic.h>
#include <init/initfn.h>
#include <limine.h>
#include <mm/hhdm.h>

static volatile __used __section(".limine_requests") struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0,
};

uintptr_t rsdp_address = 0;

static int init_rsdp(void)
{
    if(rsdp_request.response == nullptr || rsdp_request.response->address == 0) {
        panic("bootloader did not provide RSDP information");
        unreachable();
    }

    rsdp_address = virt_to_phys(rsdp_request.response->address);

    return 0;
}

DEFINE_INITFN(init_rsdp, INITFN_STAGE_BOOTINFO, INITFN_ORDER_LAST);
