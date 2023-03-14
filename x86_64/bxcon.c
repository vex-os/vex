/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <x86_64/pmio.h>

static void bxcon_putchar(console_t *restrict console, int c)
{
    pmio_write8(0xE9, c);
}

static console_t bxcon = {
    .driver_data = NULL,
    .driver_name = "bxcon",
    .putchar = &bxcon_putchar,
    .unblank = NULL,
    .next = NULL,
};

static void init_bxcon(void)
{
    uint8_t result;
    if((pmio_read8(0xE9, &result) == 0) && (result == 0xE9)) {
        register_console(&bxcon);
        return;
    }
}
early_initcall(bxcon, init_bxcon);
