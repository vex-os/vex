/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/debug.h>
#include <kaneru/kprintf.h>
#include <sprintf.h>
#include <string.h>

void __noreturn panicv_impl(const char *restrict file, unsigned long line, const char *restrict fmt, va_list ap)
{
    kvprintf_impl(KP_UNDEFINED, file, line, fmt, ap);
    kputs(KP_UNDEFINED, "kernel panic");
    for(;;) { /* undone */ }
}

void __noreturn panic_impl(const char *restrict file, unsigned long line, const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    panicv_impl(file, line, fmt, ap);
    va_end(ap);
}
