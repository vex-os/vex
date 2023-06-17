/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kern/console.h>
#include <kern/systm.h>
#include <pfmt.h>

static void func(int c, void *restrict arg)
{
    console_putchar(c);
}

void kputs(const char *restrict s)
{
    size_t i;
    for(i = 0; s[i]; console_putchar(s[i++]));
    console_putchar('\r');
    console_putchar('\n');
}

void kprintf(const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vpfmt(&func, NULL, fmt, ap);
    console_putchar('\r');
    console_putchar('\n');
    va_end(ap);
}

void kvprintf(const char *restrict fmt, va_list ap)
{
    vpfmt(&func, NULL, fmt, ap);
    console_putchar('\r');
    console_putchar('\n');
}
