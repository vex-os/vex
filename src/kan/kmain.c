/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/debug.h>
#include <kan/errno.h>
#include <kan/initcall.h>
#include <kan/interrupt.h>
#include <kan/io.h>
#include <kan/kmalloc.h>
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <kan/version.h>
#include <string.h>

static int int42_handler(interrupt_frame_t *restrict frame, void *restrict arg)
{
    pr_inform("interrupts test: handler called (int $%#02zX)", (size_t)frame->vector);
    return INTERRUPT_CONSUME;
}

void __noreturn __used kmain(void)
{
    int r;
    size_t i;
    void *p1;
    void *p2;
    void *p3;
    io_block_t e9 = { 0 };

    /* Print some information about ourselves... */
    pr_inform("starting version %s", kernel_semver);

    for(i = 0; __initcalls[i].func; i++) {
        r = __initcalls[i].func();

        /* An initcall returning ENODEV is a special case,
         * it just means the initcall wasn't successful but
         * it didn't fail either (for instance there was no
         * device associated with the driver or whatever). */
        if(r != EOK && r != ENODEV) {
            panic("%s: %s", __initcalls[i].name, strerror(r));
            unreachable();
        }
    }

    /* Test interrupt handling */
    add_interrupt_handler(alloc_interrupt(0x42), &int42_handler, NULL);
    add_interrupt_handler(alloc_interrupt(0x43), &int42_handler, NULL);
    add_interrupt_handler(alloc_interrupt(0x44), &int42_handler, NULL);
    asm volatile("int $0x42");
    asm volatile("int $0x43");
    asm volatile("int $0x44");

    /* Test export table */
    pr_inform("kputs: export=%p kernel=%p", get_export("kputs"), (void *)(&kputs));
    pr_inform("kmalloc: export=%p kernel=%p", get_export("kmalloc"), (void *)(&kmalloc));

    /* Test kmalloc */
    p1 = kmalloc(64);
    p2 = kmalloc(64);
    pr_inform("kmalloc test 1: p1=%p p2=%p", p1, p2);
    kfree(p1);
    p3 = kmalloc(64);
    pr_inform("kmalloc test 2: p1=%p p3=%p", p1, p3);
    kfree(p2);
    kfree(p3);
    p1 = kmalloc(8192);
    pr_inform("kmalloc test 3: p1=%p", p1);
    kfree(p1);

    /* Test IO */
    e9.base = 0x00E9;
    e9.size = 1;
    e9.mode = IO_PORT_MAPPED;
    pr_inform("io test: writing string to pmio %p", (void *)e9.base);
    io_write8(&e9, 0, 'T');
    io_write8(&e9, 0, 'E');
    io_write8(&e9, 0, 'S');
    io_write8(&e9, 0, 'T');
    io_write8(&e9, 0, ' ');
    io_write8(&e9, 0, 'I');
    io_write8(&e9, 0, 'O');
    io_write8(&e9, 0, 012);

    panic("nothing to do");
    unreachable();
}
