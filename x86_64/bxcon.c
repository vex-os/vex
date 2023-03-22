/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <x86_64/pmio.h>

static console_t bxcon = { 0 };

static void bxcon_putchar(console_t *restrict console, int c)
{
    pmio_write8(0xE9, c);
}

static void init_bxcon(void)
{
    uint8_t result;
    if((pmio_read8(0xE9, &result) == 0) && (result == 0xE9)) {
        bxcon.con_next = NULL;
        bxcon.con_putchar = &bxcon_putchar;
        bxcon.con_unblank = NULL;
        bxcon.con_name = "bxcon";
        bxcon.con_data = NULL;
        register_console(&bxcon);
        return;
    }
}
early_initcall(bxcon, init_bxcon);
