/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_KAN_CONSOLE_H__
#define _INCLUDE_KAN_CONSOLE_H__
#include <uapi/kan/console.h>

int register_console(console_t *restrict con);
void unregister_console(console_t *restrict con);
void console_write(const void *restrict s, size_t n);

#endif /* _INCLUDE_KAN_CONSOLE_H__ */
