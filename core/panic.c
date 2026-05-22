// SPDX-License-Identifier: BSD-3-Clause

#include <arch/halt.h>
#include <arch/interrupts.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <dev/console.h>

void __noreturn fpanic(const char* restrict file, unsigned long line, const char* restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fvpanic(file, line, fmt, ap);
    unreachable();
}

void __noreturn fvpanic(const char* restrict file, unsigned long line, const char* restrict fmt, va_list ap)
{
    disable_interrupts();

    console_unblank_all();

    kprintf(KP_EMERG, "panic: at %s:%lu", file, line);
    kvprintf(KP_EMERG, fmt, ap);

    for(;;) {
        disable_interrupts();
        halt_cpu();
        continue;
    }

    unreachable();
}
