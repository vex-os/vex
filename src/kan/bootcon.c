/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, Kirill GPRB */
#include <kan/console.h>
#include <kan/errno.h>
#include <kan/initcall.h>
#include <limine.h>

static volatile __used struct limine_terminal_request terminal_request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .revision = 0,
    .response = NULL,
    .callback = NULL,
};

static void bootcon_write(console_t *restrict con, const void *restrict s, size_t n)
{
    uint64_t i;
    for(i = 0; i < terminal_request.response->terminal_count; i++)
        terminal_request.response->write(terminal_request.response->terminals[i], s, (uint64_t)n);
}

static console_t bootcon = {
    .name = "limine_terminal",
    .init_fn = NULL,
    .write_fn = &bootcon_write,
    .next = NULL,
};

static int init_bootcon(void)
{
    if(terminal_request.response)
        return register_console(&bootcon);
    return ENODEV;
}
initcall_tier_0(bootcon, init_bootcon);
