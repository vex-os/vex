/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/debug.h>
#include <kaneru/interrupt.h>
#include <kaneru/kprintf.h>
#include <kaneru/trace.h>
#include <sprintf.h>
#include <stdint.h>
#include <string.h>

void __noreturn vpanic(const char *restrict fmt, va_list ap)
{
    static char buffer[1024] = { 0 };

    disable_interrupts();

    print_backtrace(KP_UNMASKABLE, NULL);

    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    kprintf(KP_UNMASKABLE, "panic: %s", buffer);

    for(;;) {
        disable_interrupts();
        asm volatile("hlt");
    }
}

void __noreturn panic(const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vpanic(fmt, ap);
    va_end(ap);
}
