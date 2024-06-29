// SPDX-License-Identifier: Zlib
#include <arch/bxcon.h>
#include <arch/pmio.h>
#include <kern/console.h>
#include <stddef.h>

static void bxcon_write(struct console *restrict con, const void *buf, size_t sz)
{
    size_t i;
    const char *psz = buf;
    for(i = 0; i < sz; pmio_write8(0xE9, psz[i++]));
}

static struct console bxcon = {
    .cs_write = &bxcon_write,
    .cs_identity = "bxcon",
    .cs_flags = CS_PRINTBUFFER,
};

void init_bxcon(void)
{
    if(pmio_read8(0xE9) != 0xE9)
        return;
    register_console(&bxcon);
}
