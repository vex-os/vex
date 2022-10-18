/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/debug.h>
#include <kaneru/errno.h>
#include <kaneru/initcall.h>
#include <kaneru/interrupt.h>
#include <kaneru/kprintf.h>
#include <kaneru/version.h>
#include <stddef.h>
#include <string.h>
#include <limine.h>

static void kmain(void) __noreturn;
static volatile struct limine_entry_point_request __used kmain_request = {
    .id = LIMINE_ENTRY_POINT_REQUEST,
    .revision = 0,
    .response = NULL,
    .entry = (limine_entry_point)(&kmain),
};

static void test_intr(void)
{
    kprintf(KP_INTERRUPT, "testing!");
    x86_disable_interrupts();
}

static void __noreturn kmain(void)
{
    int errnum;
    const struct initcall *ic;

    /* Print version */
    kprintf(KP_INITIAL, "kmain: starting version %s", K_SEMVER);

    /* Initialize all the subsystems */
    for(ic = &__initcalls[0]; ic->func && ic->name[0]; ic++) {
        errnum = ic->func();
        if(errnum != 0) {
            if(errnum == -ENODEV)
                continue;
            panic("init %s: %s", ic->name, strerror(-errnum));
        }
    }

    /* test interrupts */
    int test_vec;
    if(alloc_interrupt(&test_vec, -1)) {
        add_interrupt_handler(test_vec, &test_intr);
        x86_map_interrupt(test_vec, 0x80, false);
        x86_enable_interrupts();
        asm volatile("int $0x80");
    }

    panic("nothing to do");
}
