/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/console.h>
#include <kan/errno.h>
#include <kan/initcall.h>
#include <limine.h>
#include <stddef.h>

static volatile struct limine_terminal_request terminal_request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .callback = NULL,
    .response = NULL,
    .revision = 0,
};

static void bootcon_write(console_t *restrict con, const void *restrict str, size_t size)
{
    size_t i;
    struct limine_terminal *terminal;
    for(i = 0; i < terminal_request.response->terminal_count; i++) {
        terminal = terminal_request.response->terminals[i];
        terminal_request.response->write(terminal, str, size);
    }
}

static console_t bootcon = {
    .name = "limine_terminal",
    .write = &bootcon_write,
    .next = NULL,
};

static int init_bootcon(void)
{
    (void)bootcon;
    if(terminal_request.response)
        return register_console(&bootcon);
    return ENODEV;
}
initcall_tier_0(bootcon, init_bootcon);
