/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_PRINTF_H__
#define __INCLUDE_SYS_PRINTF_H__
#include <stdarg.h>
#include <sys/cdefs.h>

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

#endif /* __INCLUDE_SYS_PRINTF_H__ */
