/* SPDX-License-Identifier: Zlib */
#include <arch/cpu.h>
#include <vex/console.h>
#include <vex/panic.h>
#include <vex/printf.h>

void NORETURN fpanic(const char *restrict file, long line, const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fvpanic(file, line, fmt, ap);
    UNREACHABLE();
    va_end(ap);
}

void NORETURN fvpanic(const char *restrict file, long line, const char *restrict fmt, va_list ap)
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
