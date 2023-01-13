/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, Kirill GPRB */
#include <kan/boot.h>
#include <kan/console.h>
#include <kan/errno.h>
#include <kan/initcall.h>

static void bootcon_write(console_t *restrict con, const void *restrict s, size_t n)
{
    uint64_t i;
    struct limine_terminal *terminal;
    for(i = 0; i < boot_terminal.response->terminal_count; i++) {
        terminal = boot_terminal.response->terminals[i];
        boot_terminal.response->write(terminal, s, (uint64_t)(n));
    }
}

static console_t bootcon = {
    .name = "limine_terminal",
    .init_fn = NULL,
    .write_fn = &bootcon_write,
    .next = NULL,
};

static int init_bootcon(void)
{
    if(boot_terminal.response)
        return register_console(&bootcon);
    return ENODEV;
}
initcall_tier_0(bootcon, init_bootcon);
