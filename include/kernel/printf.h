/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023, VX/sys Contributors */
#ifndef INCLUDE_KERNEL_PRINTF_H
#define INCLUDE_KERNEL_PRINTF_H
#include <stdarg.h>
#include <vx/cdefs.h>

struct console {
    struct console *con_next;
    void (*con_putchar)(struct console *restrict con, int c);
    void (*con_unblank)(struct console *restrict con);
    char con_identity[64];
    void *con_custom;
};

int add_console(struct console *restrict con);
int remove_console(struct console *restrict con);
void unblank_consoles(void);

void kputchar(int c);
void kputs(const char *restrict s);
void kprintf(const char *restrict fmt, ...) __printflike(1, 2);
void kvprintf(const char *restrict fmt, va_list ap) __printflike(1, 0);

#endif /* INCLUDE_KERNEL_PRINTF_H */
