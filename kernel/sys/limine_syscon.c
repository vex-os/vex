/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/errno.h>
#include <kaneru/initcall.h>
#include <kaneru/kprintf.h>
#include <kaneru/syscon.h>
#include <limine.h>

static volatile struct limine_terminal_request __used lterm_request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .revision = 0,
    .response = NULL,
    .callback = NULL,
};

static int limine_syscon_init(struct sys_console *console)
{
    if(lterm_request.response && lterm_request.response->terminal_count) {
        kprintf(KP_CONSOLE, "limine_syscon: init with terminal_count=%zu", (size_t)lterm_request.response->terminal_count);
        console->driver_data = lterm_request.response;
        return 0;
    }

    return -ENODEV;
}

static void limine_syscon_write(struct sys_console *console, const void *restrict s, size_t n)
{
    uint64_t terminal;
    struct limine_terminal_response *response;

    if((response = console->driver_data)) {
        for(terminal = 0; terminal < response->terminal_count; terminal++) {
            response->write(response->terminals[terminal], s, (uint64_t)n);
        }
    }
}

static struct sys_console limine_syscon = {
    .name = "limine_syscon",
    .driver_data = NULL,
    .init_fn = &limine_syscon_init,
    .write_fn = &limine_syscon_write,
};

static int init_limine_syscon(void)
{
    return register_console(&limine_syscon);
}
initcall_tier_0(limine_syscon, init_limine_syscon);
