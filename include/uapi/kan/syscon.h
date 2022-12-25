/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_UAPI_KAN_SYSCON_H__
#define _INCLUDE_UAPI_KAN_SYSCON_H__
#include <kan/compiler.h>
#include <stddef.h>

struct syscon_s;
typedef int(*syscon_init_t)(struct syscon_s *restrict con);
typedef void(*syscon_write_t)(struct syscon_s *restrict con, const void *restrict s, size_t n);

typedef struct syscon_s {
    char name[64];
    syscon_init_t init_fn;
    syscon_write_t write_fn;
    struct syscon_s *next;
} syscon_t;

#endif /* _INCLUDE_UAPI_KAN_SYSCON_H__ */
