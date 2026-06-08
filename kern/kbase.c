#include <limine.h>
#include <sys/initcall.h>
#include <sys/kbase.h>
#include <sys/panic.h>

uintptr_t kbase_phys;
uintptr_t kbase_virt;

static volatile __used __section(".limine_requests") struct limine_executable_address_request kbase_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0,
};

static void init_kbase(void)
{
    if(kbase_request.response == nullptr) {
        panic("bootloader did not provide executable address information");
    }

    kbase_phys = kbase_request.response->physical_base;
    kbase_virt = kbase_request.response->virtual_base;
}

initcall_bsp_bootinfo(init_kbase, INIT_FIRST);
