#include <vex/kbase.h>
#include <vex/limine.h>
#include <vex/panic.h>

#include <stddef.h>

static volatile __limine_request struct limine_executable_address_request request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST,
    .revision = 0,
};

uintptr_t kbase_phys = 0U;
uintptr_t kbase_virt = 0U;

void init_kbase(void)
{
    if(request.response == NULL) {
        panic("kbase: limine_executable_address_request has no response");
        unreachable();
    }

    kbase_phys = request.response->physical_base;
    kbase_virt = request.response->virtual_base;
}
