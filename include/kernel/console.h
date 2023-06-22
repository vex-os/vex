/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KERNEL_CONSOLE_H__
#define __INCLUDE_KERNEL_CONSOLE_H__
#include <kan/cdefs.h>
#include <stddef.h>

struct console {
    struct console *con_next;
    void (*con_putchar)(struct console *restrict con, int c);
    void (*con_unblank)(struct console *restrict con);
    char con_ident[64];
    void *con_data;
};

int register_console(struct console *restrict con);
int unregister_console(struct console *restrict con);
void console_putchar(int c);
void console_unblank(void);

#endif /* __INCLUDE_KERNEL_CONSOLE_H__ */
