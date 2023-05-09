/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __SYS_HEAP_H__
#define __SYS_HEAP_H__
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/initcall.h>

void *malloc(size_t n);
void *calloc(size_t count, size_t n);
void *realloc(void *restrict ptr, size_t n);
void free(void *restrict ptr);

initcall_extern(heap);

#endif /* __SYS_HEAP_H__ */
