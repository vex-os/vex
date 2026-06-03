#ifndef B2E72E76_5D2F_43BF_A8B3_80677A7ECE4C
#define B2E72E76_5D2F_43BF_A8B3_80677A7ECE4C
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

#ifndef PDMA_APPROX_END
#define PDMA_APPROX_END 0xFFFFFF
#endif

struct pdma_stats {
    size_t total_bytes;
    size_t total_pages;
    size_t used_bytes;
    size_t used_pages;
};

extern uintptr_t pdma_alloc(size_t numpages) __nodiscard;
extern void* pdma_alloc_virt(size_t numpages) __nodiscard;
extern void pdma_free(uintptr_t physaddr, size_t numpages);
extern void pdma_free_virt(void* restrict virtaddr, size_t numpages);
extern void pdma_get_stats(struct pdma_stats* stats);

#endif /* B2E72E76_5D2F_43BF_A8B3_80677A7ECE4C */
