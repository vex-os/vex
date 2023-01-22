/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/console.h>
#include <kan/initcall.h>
#include <x86/pmio.h>

static void e9con_puts(console_t *restrict con, const char *restrict s)
{
    size_t i;
    for(i = 0; s[i]; x86_pmio_write8(0xE9, s[i++]));
}

static console_t e9con = {
    .name = "bochs_e9_console",
    .puts_fn = &e9con_puts,
    .next = NULL,
};

static int init_e9con(void)
{
    uintmax_t c;
    if(x86_pmio_read8(0xE9, &c) == 0 && c == 0xE9)
        return register_console(&e9con);
    return ENODEV;
}
initcall_tier_0(e9con, init_e9con);
