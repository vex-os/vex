/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_VEX_SLAB_H
#define INCLUDE_VEX_SLAB_H
#include <vex/initcall.h>
#include <stddef.h>

void *slab_alloc(size_t sz);
void *slab_calloc(size_t count, size_t sz);
void *slab_realloc(void *restrict ptr, size_t sz);
void slab_free(void *restrict ptr);

INITCALL_EXTERN(slab);

#endif /* INCLUDE_VEX_SLAB_H */
