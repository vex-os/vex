/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KERN_SLAB_H__
#define __INCLUDE_KERN_SLAB_H__
#include <kern/init.h>
#include <stddef.h>

void *slab_alloc(size_t n);
void *slab_calloc(size_t count, size_t n);
void *slab_realloc(void *restrict ptr, size_t n);
void slab_free(void *restrict ptr);

initcall_extern(slab);

#endif /* __INCLUDE_KERN_SLAB_H__ */