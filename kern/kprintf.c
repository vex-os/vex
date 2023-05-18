/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kan/console.h>
#include <kan/system.h>
#include <printf.h>
#include <string.h>
#include <strings.h>

static void fcprintf_func(int c, void *restrict arg)
{
    console_putchar(c);
}

void kputs(const char *restrict s)
{
    size_t i;
    for(i = 0; s[i]; console_putchar(s[i++]));
    console_putchar('\n');
}

void kprintf(const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fcvprintf(&fcprintf_func, NULL, fmt, ap);
    console_putchar('\n');
    va_end(ap);
}

void kvprintf(const char *restrict fmt, va_list ap)
{
    fcvprintf(&fcprintf_func, NULL, fmt, ap);
    console_putchar('\n');
}
