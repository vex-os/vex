/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_ISERIX_SLAB_H
#define INCLUDE_ISERIX_SLAB_H
#include <iserix/initcall.h>
#include <stddef.h>

void *slab_alloc(size_t sz);
void *slab_calloc(size_t count, size_t sz);
void *slab_realloc(void *restrict ptr, size_t sz);
void slab_free(void *restrict ptr);

INITCALL_EXTERN(slab);

#endif /* INCLUDE_ISERIX_SLAB_H */
