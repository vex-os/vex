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
    pr_inform(KP_UNMASKABLE, "starting version %s", kernel_semver);

    for(i = 0; __initcalls[i].func; i++) {
        if((r = __initcalls[i].func()) < 0 && r != -ENODEV) {
            panic("%s: %s", __initcalls[i].name, strerror(r));
            unreachable();
        }
    }

    panic("nothing to do");
    unreachable();
}
