#include <arch/io.h>
#include <vex/console.h>
#include <vex/initcall.h>

static void bxcons_write(console_t* restrict cons, const void* restrict data, size_t size)
{
    const char* psz = data;

    for(size_t i = 0; i < size; ++i) {
        port_write8(0xE9, psz[i]);
    }
}

static const console_ops_t bxcons_ops = {
    .write = &bxcons_write,
};

static console_t bxcons_driver = {
    .ops = &bxcons_ops,
    .name = "bxcons",
    .consflags = CONSF_RECALL | CONSF_LOGLEVEL,
    .loglevel = KP_VERBOSE,
};

static void arch_init_bxcons(void)
{
    if(0xE9 == port_read8(0xE9)) {
        console_add(&bxcons_driver);
    }
}

initcall_bsp_early(arch_init_bxcons, INIT_FIRST);
