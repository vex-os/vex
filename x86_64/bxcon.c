/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <kern/console.h>
#include <kern/initcall.h>
#include <kern/vmm.h>
#include <stddef.h>
#include <x86_64/pmio.h>

static void bxcon_write(struct console *con, const void *restrict buf, size_t sz)
{
    size_t i;
    const char *cbuf = buf;
    for(i = 0; i < sz; pmio_write8(0xE9, cbuf[i++]));
}

static struct console bxcon = {
    .cs_next = NULL,
    .cs_write = &bxcon_write,
    .cs_unblank = NULL,
    .cs_identity = "bxcon",
    .cs_flag = CON_PRINTBUFFER,
    .cs_private = NULL,
};

static void init_bxcon(void)
{
    if(pmio_read8(0xE9) != 0xE9)
        return;
    register_console(&bxcon);
}
core_initcall(bxcon, init_bxcon);
initcall_depend(vmm, bxcon); /* crutch, remove asap */
