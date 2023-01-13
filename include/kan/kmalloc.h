/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, Kirill GPRB */
#ifndef _INCLUDE_KAN_KMALLOC_H__
#define _INCLUDE_KAN_KMALLOC_H__
#include <kan/compiler.h>
#include <kan/initcall.h>
#include <stddef.h>

void *kmalloc(size_t n);
void *kcalloc(size_t count, size_t n);
void *krealloc(void *restrict ptr, size_t n);
char *kstrdup(const char *restrict s);
void kfree(void *restrict ptr);

#endif /* _INCLUDE_KAN_KMALLOC_H__ */
