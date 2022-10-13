/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_SYS_CONSOLE_H__
#define __INCLUDE_SYS_CONSOLE_H__
#include <stddef.h>

struct console;
typedef int(*con_init_t)(struct console *);
typedef size_t(*con_write_t)(struct console *, const void *, size_t);

struct console {
    char name[32];
    void *driver_data;
    con_init_t init_fn;
    con_write_t write_fn;
    struct console *next;
};

void register_console(struct console *con);
void set_active_console(struct console *con);
size_t console_write(const void *s, size_t n);

#endif /* __INCLUDE_SYS_CONSOLE_H__ */
