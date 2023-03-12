/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <machine/cpu.h>
#include <printf.h>
#include <string.h>
#include <sys/system.h>

void __noreturn panic(const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vpanic(fmt, ap);
    UNREACHABLE();
    va_end(ap);
}

void __noreturn vpanic(const char *restrict fmt, va_list ap)
{
    // This will be used only once
    // during the kernel's lifetime.
    static char stage[512] = { 0 };

    disable_interrupts();

    memset(stage, 0, sizeof(stage));
    vsnprintf(stage, sizeof(stage), fmt, ap);
    kprintf("panic: %s", stage);

    for(;;) {
        idle();
        disable_interrupts();
        continue;
    }

    UNREACHABLE();
}
