/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_MM_SLAB_H
#define INCLUDE_MM_SLAB_H
#include <kern/compiler.h>
#include <stddef.h>

void *slab_alloc(size_t sz);
void *slab_calloc(size_t count, size_t sz);
void *slab_realloc(void *restrict ptr, size_t sz);
void slab_free(void *restrict ptr);

void init_slab(void);

#endif /* INCLUDE_MM_SLAB_H */
