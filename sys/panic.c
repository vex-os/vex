/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <machine/cpu.h>
#include <sys/panic.h>
#include <sys/printf.h>

void fpanic(const char *restrict file, long line, const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fvpanic(file, line, fmt, ap);
    unreachable();
    va_end(ap);
}

void fvpanic(const char *restrict file, long line, const char *restrict fmt, va_list ap)
{
    disable_interrupts();
    unblank_consoles();

    kprintf("panic at [%s:%ld]", file, line);
    kvprintf(fmt, ap);

    /* undone: backtrace */
    /* backtrace(NULL); */

    for(;;) {
        halt_cpu();
        disable_interrupts();
        continue;
    }

    unreachable();
}
