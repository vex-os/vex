/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#ifndef INCLUDE_SYS_CONSOLE_H
#define INCLUDE_SYS_CONSOLE_H
#include <stddef.h>
#include <sys/cdefs.h>

#define CON_PRINTBUFFER 0x0001
#define CON_BLANKED     0x0002

struct console {
    struct console *cs_next;
    void (*cs_write)(struct console *restrict con, const void *restrict buf, size_t sz);
    void (*cs_unblank)(struct console *restrict con);
    char cs_identity[32];
    unsigned int cs_flag;
    void *cs_private;
};

int register_console(struct console *restrict con);
int unregister_console(struct console *restrict con);
void console_write(const void *restrict buf, size_t sz);
void console_unblank(void);

#endif /* INCLUDE_SYS_CONSOLE_H */
