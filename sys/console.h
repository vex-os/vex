/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __SYS_CONSOLE_H__
#define __SYS_CONSOLE_H__
#include <sys/cdefs.h>

typedef struct console_s {
    struct console_s *next;
    void (*putchar)(struct console_s *restrict console, int c);
    void (*unblank)(struct console_s *restrict console);
    char ident[64];
    void *data;
} console_t;

int register_console(console_t *restrict console);
void unregister_console(console_t *restrict console);
void console_putchar(int c);
void console_unblank(void);

#endif /* __SYS_CONSOLE_H__ */
