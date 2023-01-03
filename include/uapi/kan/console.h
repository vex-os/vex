/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_UAPI_KAN_CONSOLE_H__
#define _INCLUDE_UAPI_KAN_CONSOLE_H__
#include <kan/compiler.h>
#include <stddef.h>

struct console_s;
typedef int(*console_init_t)(struct console_s *restrict con);
typedef void(*console_write_t)(struct console_s *restrict con, const void *restrict s, size_t n);

typedef struct console_s {
    char name[64];
    console_init_t init_fn;
    console_write_t write_fn;
    struct console_s *next;
} console_t;

#endif /* _INCLUDE_UAPI_KAN_CONSOLE_H__ */
