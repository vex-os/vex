// SPDX-License-Identifier: BSD-3-Clause

#include <amd64/pmio.h>
#include <core/console.h>
#include <core/errno.h>
#include <init/initfn.h>

#define BXCONSOLE_PORT 0xE9
#define BXCONSOLE_RESP 0xE9

static void bxconsole_write(struct console_driver* restrict driver, const void* restrict data, size_t size)
{
    size_t i;
    const char* psz = data;

    for(i = 0; i < size; ++i) {
        pmwrite8(BXCONSOLE_PORT, psz[i]);
    }
}

static const struct console_driver_ops bxconsole_ops = {
    .init = nullptr,
    .write = bxconsole_write,
    .unblank = nullptr,
};

static struct console_driver bxconsole_driver = {
    .next = nullptr,
    .ops = &bxconsole_ops,
    .consflags = CONSF_RECALL,
    .identity = "bxconsole",
    .privdata = nullptr,
};

static int init_bxconsole(void)
{
    if(BXCONSOLE_RESP == pmread8(BXCONSOLE_PORT)) {
        return register_console_driver(&bxconsole_driver);
    }

    return ENODEV;
}

DEFINE_INITFN(init_bxconsole, INITFN_STAGE_EARLY, INITFN_ORDER_FIRST);
