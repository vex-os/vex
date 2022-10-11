/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/debug.h>
#include <kaneru/kprintf.h>
#include <sprintf.h>

void __noreturn panicv(const char *fmt, va_list va)
{
    static char msg[1024] = { 0 };
    vsnprintf(msg, sizeof(msg), fmt, va);
    pr_emerg("panic: %s", msg);
    for(;;) { /* big mistake */ }
}

void __noreturn panic(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    panicv(fmt, va);
    va_end(va);
}
