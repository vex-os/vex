#include <arch/bxcons.h>
#include <arch/pmio.h>

#include <vex/consdrv.h>
#include <vex/errno.h>

#ifndef CONF_DEV_BXCONS_PORT
#define CONF_DEV_BXCONS_PORT 0xE9
#endif

#ifndef CONF_DEV_BXCONS_SIGNATURE
#define CONF_DEV_BXCONS_SIGNATURE 0xE9
#endif

static void bxcons_write(struct consdrv* driver, const void* restrict data, size_t sz)
{
    size_t i;
    const char* psz = data;

    for(i = 0; i < sz; ++i) {
        pmwrite8(CONF_DEV_BXCONS_PORT, psz[i]);
    }
}

static struct consdrv_ops bxcons_ops = {
    .init = NULL,
    .write = &bxcons_write,
    .unblank = NULL,
};

static struct consdrv bxcons_driver = {
    .flags = CONSF_RECALL,
    .identity = "bxcons",
    .ops = &bxcons_ops,
};

void init_bxcons(void)
{
    if(CONF_DEV_BXCONS_SIGNATURE == pmread8(CONF_DEV_BXCONS_PORT)) {
        consdrv_add(&bxcons_driver);
    }
}
