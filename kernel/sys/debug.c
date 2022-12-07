/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/debug.h>
#include <kaneru/kprintf.h>
#include <kaneru/trace.h>
#include <sprintf.h>
#include <stdint.h>
#include <string.h>

void __noreturn panicv_impl(const char *restrict file, unsigned long line, const char *restrict fmt, va_list ap)
{
    static char buffer[1024] = { 0 };

    print_stack_backtrace(get_base_pointer());

    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    kprintf(KP_UNMASKABLE, "panic at %s:%lu: %s", file, line, buffer);

    for(;;) {
        asm volatile(__string_va(
            cli;
            hlt;
        ));
    }
}

void __noreturn panic_impl(const char *restrict file, unsigned long line, const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    panicv_impl(file, line, fmt, ap);
    va_end(ap);
}
