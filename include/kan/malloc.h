/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KAN_MALLOC_H__
#define __INCLUDE_KAN_MALLOC_H__
#include <kan/initcall.h>
#include <stddef.h>

void *malloc(size_t n);
void *calloc(size_t count, size_t n);
void *realloc(void *restrict ptr, size_t n);
void free(void *restrict ptr);

initcall_extern(malloc);

#endif /* __INCLUDE_KAN_MALLOC_H__ */
