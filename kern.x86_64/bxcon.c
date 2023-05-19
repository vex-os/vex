/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <arch/pmio.h>
#include <kan/console.h>
#include <kan/initcall.h>

static void bxcon_write(console_t *restrict cons, const void *restrict s, size_t n)
{
    size_t i;
    const char *sp = s;
    for(i = 0; i < n; pmio_write8(0xE9, sp[i++]));
}

static console_t bxcon = {
    .next = NULL,
    .ident = "bxcon",
    .driver_data = NULL,
    .flags = (CONSF_PRINTHIST),
    .write = &bxcon_write,
    .unblank = NULL,
};

static void init_bxcon(void)
{
    if(pmio_read8(0xE9) != 0xE9)
        return;
    register_console(&bxcon);
}
early_initcall(bxcon, init_bxcon);
