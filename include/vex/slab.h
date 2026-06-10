#ifndef B0FA8C47_F93D_43E0_A222_9A9C29CDA2B7
#define B0FA8C47_F93D_43E0_A222_9A9C29CDA2B7
#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#ifdef TRACE_SLAB
#include <vex/kprintf.h>
#endif

void* slab_alloc_notrace(size_t sz) __nodiscard;
void* slab_realloc_notrace(void* restrict ptr, size_t sz) __nodiscard;
void slab_free_notrace(void* restrict ptr);

#ifdef TRACE_SLAB
#define slab_alloc(sz) (pr_verbose("%s:%lu: slab_alloc %zu", __FILE__, (unsigned long)(__LINE__), sz), slab_alloc_notrace(sz))
#define slab_realloc(ptr, sz) \
    (pr_verbose("%s:%lu: slab_realloc %p, %zu", __FILE__, (unsigned long)(__LINE__), ptr, sz), slab_realloc_notrace(ptr, sz))
#define slab_free(ptr) (pr_verbose("%s:%lu: slab_free %p", __FILE__, (unsigned long)(__LINE__), ptr), slab_free_notrace(ptr))
#else
#define slab_alloc(sz)        slab_alloc_notrace(sz)
#define slab_realloc(ptr, sz) slab_realloc_notrace(ptr, sz)
#define slab_free(ptr)        slab_free_notrace(ptr)
#endif

#endif /* B0FA8C47_F93D_43E0_A222_9A9C29CDA2B7 */
