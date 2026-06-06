#include <bus/acpi_rsdp.h>
#include <core/panic.h>
#include <init/initcall.h>
#include <limine.h>
#include <mm/hhdm.h>

static volatile __used __section(".limine_requests") struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0,
};

uintptr_t rsdp_address = 0;

define_initcall(acpi_rsdp, INITCALL_STAGE_BSP_BOOTINFO, INITCALL_ORDER_LAST)
{
    if(rsdp_request.response == nullptr || rsdp_request.response->address == 0) {
        panic("bootloader did not provide RSDP information");
    }

    rsdp_address = virt_to_phys(rsdp_request.response->address);
}
