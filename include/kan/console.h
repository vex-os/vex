/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_KAN_CONSOLE_H__
#define _INCLUDE_KAN_CONSOLE_H__
#include <kan/compiler.h>
#include <stddef.h>

typedef struct console_s {
    char name[64];
    void(*puts_fn)(struct console_s *restrict con, const char *restrict s);
    struct console_s *next;
} console_t;

int register_console(console_t *restrict con);
void unregister_console(console_t *restrict con);
void console_puts(const char *restrict s);

#endif /* _INCLUDE_KAN_CONSOLE_H__ */
