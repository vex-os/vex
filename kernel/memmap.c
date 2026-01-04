#include <vex/memmap.h>
#include <vex/panic.h>

#include <stddef.h>

static volatile __limine_request struct limine_memmap_request request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};

const struct limine_memmap_response* memmap = NULL;

void init_memmap(void)
{
    if(request.response == NULL) {
        panic("memmap: limine_memmap_request has no response");
        unreachable();
    }

    memmap = request.response;
}
