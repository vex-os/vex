/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <limine.h>
#include <sys/initcall.h>
#include <sys/klog.h>
#include <sys/version.h>

static void kmain(void) __noreturn;
static volatile struct limine_entry_point_request __used boot_entry_point = {
    .id = LIMINE_ENTRY_POINT_REQUEST,
    .revision = 0,
    .response = NULL,
    .entry = (limine_entry_point)(&kmain),
};

static void __noreturn kmain(void)
{
    unsigned int i;

    /* print semver */
    klog(KL_INIT, "kernel version %s", SEMVER);

    /* initialize everything */
    for(i = 0; __initcalls[i]; __initcalls[i++]());

    /* nothing else to do now */
    klog(KL_INIT, "nothing to do!");
    for(;;);
}
