#pragma once

#include <vex/compiler.h>
#include <vex/page.h>
#include <vex/types.h>

#define SLAB_MAX_SIZE (PAGE_SIZE / 2)

void* slab_alloc(size_t sz) __nodiscard;
void* slab_realloc(void* restrict ptr, size_t sz) __nodiscard;
void slab_free(void* restrict ptr);
