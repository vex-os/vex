/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#ifndef INCLUDE_KERN_CONSOLE_H
#define INCLUDE_KERN_CONSOLE_H
#include <stddef.h>
#include <sys/vx/cdefs.h>

enum console_flags {
    CON_PRINTBUFFER = 0x0001,
    CON_CONSDEV     = 0x0002,
    CON_BLANKED     = 0x0004,
};

struct console;
struct console_ops {
};

struct console {
    char co_name[32];
    unsigned int co_flags;
    void (*co_putchar)(struct console *restrict con, int ch);
    void (*co_unblank)(struct console *restrict con);
    struct console *co_next;
    void *co_data;
};

int register_console(struct console *restrict con);
int unregister_console(struct console *restrict con);
void console_putchar(int ch);
void console_unblank(void);

#endif /* INCLUDE_KERN_CONSOLE_H */
