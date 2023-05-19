/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_PMM_H__
#define __INCLUDE_SYS_PMM_H__
#include <machine/limits.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

#define PMM_ZONE_NORMAL 0x0001
#define PMM_ZONE_DMA    0x0002
#define PMM_ZONE_DMA32  0x0003

#define PMM_ZONE_BIT(zone) ((unsigned long)(1 << (zone)))
#define PMM_ZONE_NORMAL_BIT PMM_ZONE_BIT(PMM_ZONE_NORMAL)
#define PMM_ZONE_DMA_BIT    PMM_ZONE_BIT(PMM_ZONE_DMA)
#define PMM_ZONE_DMA32_BIT  PMM_ZONE_BIT(PMM_ZONE_DMA32)

int pmm_add_memblock(uintptr_t address, size_t npages, unsigned long zone_bits);
size_t pmm_get_total_memory(void);
size_t pmm_get_used_memory(void);

uintptr_t pmm_alloc(size_t npages, unsigned short zone);
void pmm_free(uintptr_t address, size_t npages);
void *pmm_alloc_hhdm(size_t npages, unsigned short zone);
void pmm_free_hhdm(void *restrict ptr, size_t npages);

static __always_inline inline uintptr_t page_align_address(uintptr_t address)
{
    return __align_floor(address, PAGE_SIZE);
}

static __always_inline inline void *page_align_pointer(void *restrict ptr)
{
    return (void *)(__align_floor((uintptr_t)ptr, PAGE_SIZE));
}

static __always_inline inline const void *page_align_const_pointer(const void *restrict ptr)
{
    return (const void *)(__align_floor((uintptr_t)ptr, PAGE_SIZE));
}

static __always_inline inline size_t get_page_count(size_t n)
{
    return __align_ceil(n, PAGE_SIZE) / PAGE_SIZE;
}

#endif /* __INCLUDE_SYS_PMM_H__ */
