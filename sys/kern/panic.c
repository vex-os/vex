/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/klog.h>
#include <kaneru/panic.h>
#include <sprintf.h>

void __noreturn panicv_impl(const char *restrict file, unsigned long line, const char *restrict fmt, va_list ap)
{
    static const char prefix[] = "kernel panic: ";
    static char buf[KL_MSGSIZE] = { 0 };
    vsnprintf(buf, sizeof(buf) - sizeof(prefix) - 1, fmt, ap);
    klog_impl(KL_UNDEF, file, line, "%s%s", prefix, buf);
    for(;;) { /* undone */ }
}

void __noreturn panic_impl(const char *restrict file, unsigned long line, const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    panicv_impl(file, line, fmt, ap);
    va_end(ap);
}
