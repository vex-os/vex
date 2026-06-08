#include <sys/initcall.h>
#include <sys/memmap.h>
#include <sys/panic.h>

const struct limine_memmap_response* memmap;

static volatile __used __section(".limine_requests") struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
};

static void init_memmap(void)
{
    if(memmap_request.response == nullptr || memmap_request.response->entry_count == 0) {
        panic("no valid memory map provided by bootloader");
    }

    memmap = memmap_request.response;
}

initcall_bsp_bootinfo(init_memmap, INIT_FIRST);
