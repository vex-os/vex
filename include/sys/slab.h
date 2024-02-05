/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#ifndef INCLUDE_SYS_SLAB_H
#define INCLUDE_SYS_SLAB_H
#include <stddef.h>
#include <sys/initcall.h>

void *slab_alloc(size_t n);
void *slab_calloc(size_t count, size_t n);
void *slab_realloc(void *restrict ptr, size_t n);
void slab_free(void *restrict ptr);

initcall_extern(slab);

#endif /* INCLUDE_SYS_SLAB_H */
