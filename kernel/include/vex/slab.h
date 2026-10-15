#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

void* slab_alloc(size_t sz) __nodiscard;
void* slab_realloc(void* restrict ptr, size_t sz) __nodiscard;
void slab_free(void* restrict ptr);
