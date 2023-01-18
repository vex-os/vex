/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _INCLUDE_KAN_KMALLOC_H__
#define _INCLUDE_KAN_KMALLOC_H__
#include <kan/compiler.h>
#include <kan/initcall.h>
#include <stddef.h>

void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void *krealloc(void *restrict ptr, size_t size);
char *kstrdup(const char *restrict str);
void kfree(void *restrict ptr);

initcall_extern(kmalloc);

#endif /* _INCLUDE_KAN_KMALLOC_H__ */
