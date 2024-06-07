/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_SYS_SLAB_H
#define _INCLUDE_SYS_SLAB_H
#include <stddef.h>
#include <sys/initcall.h>

void *slab_alloc(size_t sz);
void *slab_calloc(size_t count, size_t sz);
void *slab_realloc(void *restrict ptr, size_t );
void slab_free(void *restrict ptr);

initcall_extern(slab);

#endif /* _INCLUDE_SYS_SLAB_H */
