#include <amd64/pmio.h>
#include <core/console.h>
#include <init/initcall.h>

static void bxcons_write(struct console_driver* restrict driver, const void* restrict data, size_t size)
{
    const char* psz = data;

    for(size_t i = 0; i < size; ++i) {
        pmwrite8(0xE9, psz[i]);
    }
}

static const struct console_driver_ops bxcons_ops = {
    .init = nullptr,
    .write = &bxcons_write,
    .unblank = nullptr,
};

static struct console_driver bxcons_driver = {
    .ops = &bxcons_ops,
    .identifier = "bxcons",
    .consflags = CONSF_RECALL,
};

define_initcall(amd64_bxcons, INITCALL_STAGE_BSP_EARLY, INITCALL_ORDER_FIRST)
{
    if(0xE9 == pmread8(0xE9)) {
        console_driver_register(&bxcons_driver);
    }
}
