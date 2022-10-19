/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/debug.h>
#include <kaneru/kprintf.h>
#include <sprintf.h>
#include <string.h>

static const char *get_basename(const char *s)
{
    const char *ss = strrchr(s, '/');
    return ss ? (ss + 1) : s;
}

void __noreturn panicv_impl(const char *restrict file, unsigned long line, const char *restrict fmt, va_list ap)
{
    size_t n;
    static char buffer[KP_MSG_LENGTH] = { 0 };
    n = snprintf(buffer, sizeof(buffer), "panic at %s:%lu: ", get_basename(file), line);
    vsnprintf(buffer + n - 1, sizeof(buffer) - n, fmt, ap);
    kputs(KP_UNDEFINED, buffer);
    for(;;) { /* undone */ }
}

void __noreturn panic_impl(const char *restrict file, unsigned long line, const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    panicv_impl(file, line, fmt, ap);
    va_end(ap);
}
