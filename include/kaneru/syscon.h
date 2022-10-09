/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_SYSCON_H__
#define __INCLUDE_KANERU_SYSCON_H__
#include <stddef.h>

struct syscon;
typedef size_t(*syscon_write_t)(struct syscon *console, const void *s, size_t n);

struct syscon {
    char name[16];
    void *driver_data;
    syscon_write_t write;
    struct syscon *next;
};

void syscon_bind(struct syscon *console);
void syscon_register(struct syscon *console);
void syscon_write(const void *s, size_t n);

#endif /* __INCLUDE_KANERU_SYSCON_H__ */
