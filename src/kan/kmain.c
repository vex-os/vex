/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/debug.h>
#include <kan/errno.h>
#include <kan/initcall.h>
#include <kan/kmalloc.h>
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <kan/version.h>
#include <string.h>

void __noreturn __used kmain(void)
{
    int r;
    size_t i;
    void *p1;
    void *p2;
    void *p3;

    /* Print some information about ourselves... */
    pr_inform("starting version %s", kernel_semver);

    for(i = 0; initcalls[i].func; i++) {
        /* An initcall returning ENODEV is a special case,
         * it just means the initcall wasn't successful but
         * it didn't fail either (for instance there was no
         * device associated with the driver or whatever). */
        if((r = initcalls[i].func()) != EOK && r != ENODEV) {
            panic("%s: %s", initcalls[i].name, strerror(r));
            unreachable();
        }
    }

    /* Test export table */
    kassert(get_export("kputs") == (void *)(&kputs));
    kassert(get_export("kmalloc") == (void *)(&kmalloc));

    /* Test kmalloc */
    kassert((p1 = kmalloc(64)) != NULL);
    kassert((p2 = kmalloc(64)) != NULL);
    kfree(p1);
    kassert((p3 = kmalloc(64)) != NULL);
    kassert(p1 == p3);
    kfree(p2);
    kfree(p3);

    /* UNDONE: it's okay if it fails */
    kassert((p1 = kmalloc(2048)) != NULL);
    kfree(p1);

    panic("nothing to do");
    unreachable();
}
