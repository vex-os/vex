/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/init.h>
#include <kaneru/kprintf.h>
#include <kaneru/panic.h>
#include <kaneru/version.h>
#include <limine.h>

static void kmain(void) __noreturn;
static volatile struct limine_entry_point_request __used entrypoint_rq = {
    .id = LIMINE_ENTRY_POINT_REQUEST,
    .revision = 0,
    .response = NULL,
    .entry = (limine_entry_point)&kmain
};

static void __noreturn kmain(void)
{
    pr_inform("Kaneru version %s", KANERU_SEMVER);

    unsigned int i;
    for(i = 0; __initcalls[i]; i++) {
        /* UNDONE: check for errors */
        __initcalls[i]();
    }

    panic("Nothing to do");
}
