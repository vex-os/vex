/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/errno.h>
#include <kaneru/initcall.h>
#include <kaneru/kprintf.h>
#include <limine.h>

static volatile struct limine_terminal_request __used request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .revision = 0,
    .response = NULL,
    .callback = NULL,
};

static void console_callback(const void *restrict s, size_t n)
{
    uint64_t terminal;
    for(terminal = 0; terminal < request.response->terminal_count; terminal++) {
        request.response->write(request.response->terminals[terminal], s, (uint64_t)n);
    }
}

static int init_console(void)
{
    if(request.response && request.response->terminal_count) {
        kprintf(KP_KPRINTF, "console: init with %zu terminals", (size_t)request.response->terminal_count);
        kp_bind_callback(KP_CALLBACK_0, &console_callback);
        return 0;
    }

    return -ENODEV;
}
initcall_tier_0(console, init_console);
initcall_depend(console, kprintf);
