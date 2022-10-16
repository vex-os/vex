/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/debug.h>
#include <kaneru/errno.h>
#include <kaneru/initcall.h>
#include <kaneru/kprintf.h>
#include <kaneru/version.h>
#include <stddef.h>
#include <string.h>
#include <limine.h>

static void kmain(void) __noreturn;
static volatile struct limine_entry_point_request __used boot_entry_point = {
    .id = LIMINE_ENTRY_POINT_REQUEST,
    .revision = 0,
    .response = NULL,
    .entry = (limine_entry_point)(&kmain),
};

static int init_kversion(void)
{
    kprintf(KP_INITIAL, "kernel version %s", K_SEMVER);
    return 0;
}
initcall_tier_0(kversion, init_kversion);

static int test_initcall(void)
{
    kprintf(KP_INITIAL, "testing initcall errnum");
    return -EACCES;
}
initcall_tier_2(test, test_initcall);

static void __noreturn kmain(void)
{
    int errnum;
    unsigned int i;

    /* initialize everything */
    for(i = 0; __initcalls[i].func && __initcalls[i].name[0]; i++) {
        errnum = __initcalls[i].func();
        if(errnum != 0) {
            if(errnum == -ENODEV)
                continue;
            panic("initcall %s failed: %s", __initcalls[i].name, strerror(-errnum));
        }
    }

    panic("nothing to do");
}
