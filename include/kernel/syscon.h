/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef _INCLUDE_KERNEL_SYSCON_H__
#define _INCLUDE_KERNEL_SYSCON_H__
#include <stddef.h>
#include <sys/cdefs.h>

struct syscon_s;
typedef size_t(*syscon_read_t)(struct syscon_s *restrict con, void *restrict s, size_t n);
typedef void(*syscon_write_t)(struct syscon_s *restrict con, const void *restrict s, size_t n);

typedef struct syscon_s {
    void *driver_data;
    syscon_read_t read;
    syscon_write_t write;
} syscon_t;

void set_syscon(syscon_t *restrict con);
void syscon_write(const void *restrict s, size_t n);
size_t syscon_read(void *restrict s, size_t n);

#endif /* _INCLUDE_KERNEL_SYSCON_H__ */
