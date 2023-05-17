/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KAN_CONSOLE_H__
#define __INCLUDE_KAN_CONSOLE_H__
#include <kan/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

#define CONSF_UNBLANK   0x0001
#define CONSF_PRINTHIST 0x0002

typedef struct console_s {
    char ident[64];
    unsigned short flags;
    struct console_s *next;
    void (*write)(struct console_s *restrict cons, const void *restrict s, size_t n);
    void (*unblank)(struct console_s *restrict cons);
} console_t;

int register_console(console_t *restrict console);
int unregister_console(console_t *restrict console);
void console_putchar(int c);
void console_unblank(void);

#endif /* __INCLUDE_KAN_CONSOLE_H__ */
