/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <printf.h>
#include <sys/console.h>
#include <sys/systm.h>

static void kprintf_func(int c, void *restrict arg)
{
    console_putchar(c);
}

void kputs(const char *restrict s)
{
    while(*s)
        console_putchar(*s++);
    console_putchar('\n');
}

void kprintf(const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fcvprintf(&kprintf_func, NULL, fmt, ap);
    console_putchar('\n');
    va_end(ap);
}

void kvprintf(const char *restrict fmt, va_list ap)
{
    fcvprintf(&kprintf_func, NULL, fmt, ap);
    console_putchar('\n');
}
