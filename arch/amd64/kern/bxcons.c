#include <sys.amd64/pmio.h>
#include <sys/console.h>
#include <sys/initcall.h>

static void bxcons_write(struct console* restrict cons, const void* restrict data, size_t size)
{
    const char* psz = data;

    for(size_t i = 0; i < size; ++i) {
        pmwrite8(0xE9, psz[i]);
    }
}

static const struct console_ops bxcons_ops = {
    .write = &bxcons_write,
};

static struct console bxcons_driver = {
    .ops = &bxcons_ops,
    .identifier = "bxcons",
    .consflags = CONSF_RECALL,
};

define_initcall(amd64_bxcons, INITCALL_STAGE_BSP_EARLY, INIT_FIRST)
{
    if(0xE9 == pmread8(0xE9)) {
        add_console(&bxcons_driver);
    }
}
