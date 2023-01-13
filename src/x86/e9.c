/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, VxEngine Contributors */
#include <kan/console.h>
#include <kan/errno.h>
#include <kan/initcall.h>
#include <x86/pcres.h>

static void e9_write(console_t *restrict con, const void *restrict s, size_t n)
{
    size_t i;
    const char *sp = s;
    for(i = 0; sp[i]; resource_write8(&bochs_port_E9, 0, sp[i++]));
}

static console_t e9_console = {
    .name = "bochs_e9_console",
    .write_fn = &e9_write,
    .next = NULL,
};

static int init_e9(void)
{
    uint8_t c;
    resource_read8(&bochs_port_E9, 0, &c);
    if(c == 0xE9)
        return register_console(&e9_console);
    return ENODEV;
}
initcall_tier_0(e9, init_e9);
initcall_depend(e9, pcres);
