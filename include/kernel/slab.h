/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KERNEL_SLAB_H__
#define __INCLUDE_KERNEL_SLAB_H__
#include <kernel/init.h>
#include <stddef.h>

void *kmalloc(size_t n);
void *kcalloc(size_t count, size_t n);
void *krealloc(void *restrict ptr, size_t n);
void kfree(void *restrict ptr);

initcall_extern(slab);

#endif /* __INCLUDE_KERNEL_SLAB_H__ */
