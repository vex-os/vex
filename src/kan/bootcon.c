/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/errno.h>
#include <kan/initcall.h>
#include <kan/syscon.h>
#include <limine.h>

static volatile struct limine_terminal_request __used request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .revision = 0,
    .response = NULL,
    .callback = NULL,
};

static void bootcon_write(syscon_t *restrict con, const void *restrict s, size_t n)
{
    uint64_t term;
    for(term = 0; term < request.response->terminal_count; term++) {
        /* FIXME: do we need to check whether the write is NULL? */
        request.response->write(request.response->terminals[term], s, (uint64_t)(n));
    }
}

static syscon_t bootcon_syscon = {
    .name = "bootcon",
    .init_fn = NULL,
    .write_fn = &bootcon_write,
    .next = NULL,
};

static int init_bootcon(void)
{
    if(request.response)
        return register_syscon(&bootcon_syscon);
    return -ENODEV;
}
initcall_tier_0(bootcon, init_bootcon);
