/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_SYSCON_H__
#define __INCLUDE_KANERU_SYSCON_H__
#include <stddef.h>

#define SYSCON_DEF_INDEX (-1)
#define SYSCON_MAX_NAME (32)
#define SYSCON_AUTO_BIND (1 << 0)
#define SYSCON_AUTO_FLUSH (1 << 1)

struct syscon;
typedef void(*syscon_init_t)(struct syscon *console);
typedef size_t(*syscon_write_t)(struct syscon *console, const void *s, size_t n);

struct syscon {
    short index;
    unsigned short flags;
    char name[SYSCON_MAX_NAME];
    void *driver_data;
    syscon_init_t init;
    syscon_write_t write;
    struct syscon *next;
};

void register_syscon(struct syscon *console);
void bind_syscon(struct syscon *console);
void write_syscon(const void *s, size_t n);

#endif /* __INCLUDE_KANERU_SYSCON_H__ */
