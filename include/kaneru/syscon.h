/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_SYSCON_H__
#define __INCLUDE_KANERU_SYSCON_H__
#include <stddef.h>

struct sys_console;
typedef int(*sys_console_init_t)(struct sys_console *);
typedef size_t(*sys_console_write_t)(struct sys_console *, const void *, size_t);

struct sys_console {
    char name[32];
    void *driver_data;
    sys_console_init_t init_fn;
    sys_console_write_t write_fn;
    struct sys_console *next;
};

int register_console(struct sys_console *console);
void set_active_console(struct sys_console *console);
size_t console_write(const void *s, size_t n);

#endif /* __INCLUDE_KANERU_SYSCON_H__ */
