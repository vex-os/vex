#include <vex/hhdm.h>
#include <vex/limine.h>
#include <vex/panic.h>

#include <stddef.h>

static volatile __limine_request struct limine_hhdm_request request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
};

uintptr_t hhdm_base = 0U;

void init_hhdm(void)
{
    if(request.response == NULL) {
        panic("hhdm: limine_hhdm_request has no response");
        unreachable();
    }

    hhdm_base = request.response->offset;
}
