/* SPDX-License-Identifier: Zlib */
#include <arch/halt.h>
#include <arch/intr.h>
#include <core/console.h>
#include <core/panic.h>
#include <core/printf.h>

void __noreturn fpanic(const char *restrict file, long line, const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fvpanic(file, line, fmt, ap);
    unreachable();
    va_end(ap);
}

void __noreturn fvpanic(const char *restrict file, long line, const char *restrict fmt, va_list ap)
{
    disable_interrupts();
    console_unblank_all();

    kprintf(KP_EMERG, "panic: at %s:%ld", file, line);
    kvprintf(KP_EMERG, fmt, ap);

    /* UNDONE: at least try to get backtarce
     * at this point? Core should have this subsystem
     * in place alongside kernel symbol name resolver */

    for(;;) {
        halt_cpu();
        disable_interrupts();
        continue;
    }

    unreachable();
}
