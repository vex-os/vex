#ifndef B0FA8C47_F93D_43E0_A222_9A9C29CDA2B7
#define B0FA8C47_F93D_43E0_A222_9A9C29CDA2B7
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

void* slab_alloc(size_t sz) __nodiscard;
void* slab_calloc(size_t count, size_t sz) __nodiscard;
void* slab_realloc(void* restrict ptr, size_t sz) __nodiscard;
void slab_free(void* restrict ptr);

#endif /* B0FA8C47_F93D_43E0_A222_9A9C29CDA2B7 */
