/* SPDX-License-Identifier: Zlib */
#include <arch/pmio.h>
#include <vex/console.h>
#include <vex/initcall.h>
#include <vex/pmm.h>
#include <stddef.h>

static void bxcon_write(struct console *restrict con, const void *restrict buf, size_t sz)
{
    size_t i;
    const char *bp = buf;
    for(i = 0; i < sz; pmio_write8(0xE9, bp[i++]));
}

static struct console bxcon = {
    .cs_write = &bxcon_write,
    .cs_unblank = NULL,
    .cs_identity = "bxcon",
    .cs_flags = CS_PRINTBUFFER,
    .cs_private = NULL,
};

static void init_bxcon(void)
{
    if(pmio_read8(0xE9) != 0xE9)
        return;
    register_console(&bxcon);
}
CORE_INITCALL(x86_bxcon, init_bxcon);
INITCALL_DEPENDENCY(pmm, x86_bxcon);
