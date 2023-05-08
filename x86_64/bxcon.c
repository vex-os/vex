/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <string.h>
#include <sys/initcall.h>
#include <sys/console.h>
#include <x86_64/pmio.h>

static void bxcon_putchar(console_t *restrict console, int c)
{
    pmio_write8(0xE9, c);
}

static console_t bxcon = {
    .next = NULL,
    .putchar = &bxcon_putchar,
    .unblank = NULL,
    .ident = "bxcon",
    .data = NULL,
};

static void init_bxcon(void)
{
    if(pmio_read8(0xE9) == 0xE9) {
        register_console(&bxcon);
    }
}
early_initcall(bxcon, init_bxcon);
