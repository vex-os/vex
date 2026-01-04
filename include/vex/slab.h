#ifndef _VEX_SLAB_H
#define _VEX_SLAB_H 1

#include <vex/compiler.h>

#include <stddef.h>

extern void* slab_alloc(size_t sz) __nodiscard;
extern void* slab_calloc(size_t count, size_t sz) __nodiscard;
extern void* slab_realloc(void* restrict ptr, size_t sz) __nodiscard;
extern void slab_free(void* restrict ptr);

extern void init_slab(void);

#endif
