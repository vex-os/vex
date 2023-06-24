/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <stddef.h>
#include <sys/initcall.h>
#include <sys/printf.h>
#include <sys/vmm.h>
#include <x86_64/pmio.h>

static void bxcon_putchar(struct console *restrict con, int c)
{
    pmio_write8(0xE9, c);
}

static struct console bxcon = {
    .con_next = NULL,
    .con_putchar = &bxcon_putchar,
    .con_unblank = NULL,
    .con_identity = "bxcon",
    .con_custom = NULL,
};

static void init_bxcon(void)
{
    if(pmio_read8(0xE9) != 0xE9)
        return;
    add_console(&bxcon);
}
core_initcall(bxcon, init_bxcon);
initcall_depend(vmm, bxcon); /* crutch, remove asap */
