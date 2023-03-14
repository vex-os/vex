/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <machine/cpu.h>
#include <sys/debug.h>
#include <sys/kprintf.h>

void __noreturn fpanic(const char *restrict file, unsigned long line, const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fvpanic(file, line, fmt, ap);
    UNREACHABLE();
    va_end(ap);
}

void __noreturn fvpanic(const char *restrict file, unsigned long line, const char *restrict fmt, va_list ap)
{
    disable_interrupts();

    // Make sure all the consoles
    // that are still alive unblank
    console_unblank();

    kprintf("kernel panic at %s:%lu", file, line);
    kvprintf(fmt, ap);

    // UNDONE: backtrace
    // print_backtrace(NULL);

    for(;;) {
        idle();
        disable_interrupts();
        continue;
    }

    UNREACHABLE();
}
