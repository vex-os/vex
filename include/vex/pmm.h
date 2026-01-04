#ifndef _VEX_PMM_H
#define _VEX_PMM_H 1

#include <vex/compiler.h>

#include <stddef.h>
#include <stdint.h>

struct pmm_stats {
    size_t total_bytes;
    size_t total_pages;
    size_t used_bytes;
    size_t used_pages;
};

extern uintptr_t pmm_alloc_dma(size_t num_pages) __nodiscard;
extern void* pmm_alloc_dma_hhdm(size_t num_pages) __nodiscard;
extern void pmm_free_dma(uintptr_t address, size_t num_pages);
extern void pmm_free_dma_hhdm(void* restrict ptr, size_t num_pages);
extern void pmm_stats_dma(struct pmm_stats* restrict stats);

extern uintptr_t pmm_alloc_pslab(void) __nodiscard;
extern void* pmm_alloc_pslab_hhdm(void) __nodiscard;
extern void pmm_free_pslab(uintptr_t address);
extern void pmm_free_pslab_hhdm(void* restrict ptr);
extern void pmm_stats_pslab(struct pmm_stats* restrict stats);

extern uintptr_t pmm_alloc(void) __nodiscard;
extern void* pmm_alloc_hhdm(void) __nodiscard;
extern void pmm_free(uintptr_t address);
extern void pmm_free_hhdm(void* restrict ptr);
extern void pmm_stats(struct pmm_stats* restrict stats);

extern void init_pmm(void);

#endif
