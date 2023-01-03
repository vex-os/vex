/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/debug.h>
#include <kan/errno.h>
#include <kan/initcall.h>
#include <kan/kprintf.h>
#include <kan/version.h>
#include <string.h>

void __noreturn __used kmain(void)
{
    int r;
    size_t i;

    /* Print some information about ourselves... */
    pr_inform("starting version %s", kernel_semver);

    for(i = 0; initcalls[i].func; i++) {
        /* An initcall returning ENODEV is a special case,
         * it just means the initcall wasn't successful but
         * it didn't fail either (for instance there was no
         * device associated with the driver or whatever). */
        if((r = initcalls[i].func()) != 0 && r != ENODEV) {
            panic("%s: %s", initcalls[i].name, strerror(r));
            unreachable();
        }
    }

    panic("nothing to do");
    unreachable();
}
