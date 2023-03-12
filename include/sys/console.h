/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_CONSOLE_H__
#define __INCLUDE_SYS_CONSOLE_H__
#include <stddef.h>
#include <sys/cdefs.h>

typedef struct console_s {
    char name[64];
    void *driver_data;
    void (*write)(struct console_s *restrict console, const void *restrict s, size_t n);
    struct console_s *next;
} console_t;

int console_register(console_t *restrict console);
void console_unregister(console_t *restrict console);
void console_primary(console_t *restrict console);
void console_write(const void *restrict s, size_t n);
void console_fcprintf_func(int c, void *restrict arg);

#endif /* __INCLUDE_SYS_CONSOLE_H__ */
