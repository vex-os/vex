/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/debug.h>
#include <kaneru/kmain.h>
#include <limine.h>
#include <stddef.h>

static void kstart(void) __noreturn;
static volatile struct limine_entry_point_request __used entrypoint_rq = {
    .id = LIMINE_ENTRY_POINT_REQUEST,
    .revision = 0,
    .response = NULL,
    .entry = (limine_entry_point)&kstart
};

struct limine_framebuffer_request;

static volatile struct limine_terminal_request __used terminal_rq = {
    .id = LIMINE_TERMINAL_REQUEST,
    .revision = 0,
    .response = NULL,
    .callback = NULL,
};

static void __noreturn kstart(void)
{
    const char message[] = "Hello, Limine terminal!";
    struct limine_terminal *term;
    if(terminal_rq.response) {
        term = terminal_rq.response->terminals[0];
        terminal_rq.response->write(term, message, sizeof(message));
    }

    kmain();

    /* kmain shall not return */
    panic("kstart: unreachable code");
}
