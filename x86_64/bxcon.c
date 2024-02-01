/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <kern/console.h>
#include <kern/initcall.h>
#include <kern/vmm.h>
#include <stddef.h>
#include <x86_64/pmio.h>

static void bxcon_putchar(struct console *restrict con, int c)
{
    pmio_write8(0xE9, c);
}

static struct console bxcon = {
    .co_name = "bxcon",
    .co_flags = CON_PRINTBUFFER,
    .co_putchar = &bxcon_putchar,
    .co_unblank = NULL,
    .co_next = NULL,
    .co_data = NULL,
};

static void init_bxcon(void)
{
    if(pmio_read8(0xE9) != 0xE9)
        return;
    register_console(&bxcon);
}
core_initcall(bxcon, init_bxcon);
initcall_depend(vmm, bxcon); /* crutch, remove asap */
