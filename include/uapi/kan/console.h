/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_UAPI_KAN_CONSOLE_H__
#define _INCLUDE_UAPI_KAN_CONSOLE_H__
#include <kan/compiler.h>
#include <stddef.h>

typedef struct console_s {
    char name[64];
    void(*write)(struct console_s *restrict con, const void *restrict str, size_t size);
    struct console_s *next;
} console_t;

#endif /* _INCLUDE_UAPI_KAN_CONSOLE_H__ */
