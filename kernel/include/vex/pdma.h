#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#ifndef PDMA_APPROX_END
#define PDMA_APPROX_END 0xFFFFFF
#endif

uintptr_t pdma_alloc_phys(size_t num_pages) __nodiscard;
void* pdma_alloc_virt(size_t num_pages) __nodiscard;
void pdma_free_phys(uintptr_t phys, size_t num_pages);
void pdma_free_virt(void* restrict virt, size_t num_pages);
