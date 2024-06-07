/* SPDX-License-Identifier: GPL-2.0-only */
#include <machine/cpu.h>
#include <sys/console.h>
#include <sys/panic.h>
#include <sys/printf.h>

void fpanic(const char *restrict file, long line, const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fvpanic(file, line, fmt, ap);
    UNREACHABLE();
    va_end(ap);
}

void fvpanic(const char *restrict file, long line, const char *restrict fmt, va_list ap)
{
    disable_interrupts();
    console_unblank_all();

    kprintf(KP_EMERG, "panic: at %s:%ld", file, line);
    kvprintf(KP_EMERG, fmt, ap);

    /* undone: backtrace */
    /* backtrace(NULL); */

    for(;;) {
        halt_cpu();
        disable_interrupts();
        continue;
    }

    UNREACHABLE();
}
