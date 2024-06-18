/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_ISERIX_PMM_H
#define INCLUDE_ISERIX_PMM_H
#include <iserix/initcall.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(DMA_APPROX_END)
#define DMA_APPROX_END 0x3FFFFFF
#endif

uintptr_t dma_alloc(size_t npages);
void *dma_alloc_hhdm(size_t npages);
void dma_free(uintptr_t address, size_t npages);
void dma_free_hhdm(void *restrict ptr, size_t npages);

uintptr_t pmm_alloc(void);
void *pmm_alloc_hhdm(void);
void pmm_free(uintptr_t address);
void pmm_free_hhdm(void *restrict ptr);

INITCALL_EXTERN(pmm);

#endif /* INCLUDE_ISERIX_PMM_H */
