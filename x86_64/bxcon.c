/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kernel/console.h>
#include <kernel/init.h>
#include <kernel/vmm.h>
#include <x86_64/pmio.h>

static void bxcon_putchar(struct console *restrict con, int c)
{
    pmio_write8(0xE9, c);
}

static struct console bxcon = {
    .con_next = NULL,
    .con_putchar = &bxcon_putchar,
    .con_unblank = NULL,
    .con_ident = "bxcon",
    .con_data = NULL,
};

static void init_bxcon(void)
{
    if(pmio_read8(0xE9) != 0xE9)
        return;
    register_console(&bxcon);
}
core_initcall(bxcon, init_bxcon);
initcall_depend(vmm, bxcon); /* crutch, remove asap */
