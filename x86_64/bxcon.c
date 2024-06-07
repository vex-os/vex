/* SPDX-License-Identifier: GPL-2.0-only */
#include <stddef.h>
#include <sys/console.h>
#include <sys/initcall.h>
#include <sys/vmm.h>
#include <x86_64/pmio.h>

static void bxcon_write(struct console *restrict cons, const void *restrict buf, size_t sz)
{
    size_t i;
    const char *bp = buf;
    for(i = 0; i < sz; pmio_write8(0xE9, bp[i++]));
}

static struct console bxcon = {
    .cs_write = &bxcon_write,
    .cs_unblank = NULL,
    .cs_next = NULL,
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
core_initcall(x86_bxcon, init_bxcon);
initcall_dependency(vmm, x86_bxcon);
