// SPDX-License-Identifier: BSD-3-Clause

#include <amd64/pmio.h>
#include <core/errno.h>
#include <dev/console.h>
#include <init/initfn.h>

#define BXDEBUG_PORT 0xE9
#define BXDEBUG_RESP 0xE9

static void bxdebug_write(struct console_driver* restrict driver, const void* restrict data, size_t size)
{
    size_t i;
    const char* psz = data;

    for(i = 0; i < size; ++i) {
        pmwrite8(BXDEBUG_PORT, psz[i]);
    }
}

static const struct console_driver_ops bxdebug_ops = {
    .init = NULL,
    .write = bxdebug_write,
    .unblank = NULL,
};

static struct console_driver bxdebug_driver = {
    .next = NULL,
    .ops = &bxdebug_ops,
    .consflags = CONSF_RECALL,
    .identity = "bxdebug",
    .privdata = NULL,
};

static int init_bxdebug(void)
{
    if(BXDEBUG_RESP == pmread8(BXDEBUG_PORT)) {
        return register_console_driver(&bxdebug_driver);
    }

    return ENODEV;
}

DEFINE_INITFN(init_bxdebug, INITFN_CAT_EARLY, INITFN_ORDER_FIRST);
