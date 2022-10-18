/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_SYSCON_H__
#define __INCLUDE_KANERU_SYSCON_H__
#include <kaneru/cdefs.h>
#include <stddef.h>

/* UNDONE: unregistering consoles.
 * We might need to unregister certain
 * types of drivers after tier-0 initialization,
 * an example of this is limine terminal which
 * won't work after we reclaim bootloader memory. */

struct sys_console;
typedef int(*sys_console_init_t)(struct sys_console *);
typedef void(*sys_console_write_t)(struct sys_console *, const void *restrict, size_t);

struct sys_console {
    char name[32];
    void *driver_data;
    sys_console_init_t init_fn;
    sys_console_write_t write_fn;
    struct sys_console *next;
};

int register_console(struct sys_console *console);
void set_active_console(struct sys_console *console);
void console_write(const void *restricts, size_t n);

#endif /* __INCLUDE_KANERU_SYSCON_H__ */
