/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <string.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <x86_64/pmio.h>

static console_t bxcon = { 0 };

static void bxcn_putchar(console_t *restrict console, int c)
{
    pmio_write8(0xE9, c);
}

static void init_bxcon(void)
{
    uint8_t result;
    if((pmio_read8(0xE9, &result) == 0) && (result == 0xE9)) {
        bxcon.cn_next = NULL;
        bxcon.cn_putchar = &bxcn_putchar;
        bxcon.cn_unblank = NULL;
        kstrncpy(bxcon.cn_name, "bxcon", sizeof(bxcon.cn_name));
        bxcon.cn_data = NULL;
        register_console(&bxcon);
        return;
    }
}
early_initcall(bxcon, init_bxcon);
