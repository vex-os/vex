/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KAN_PMEM_H__
#define __INCLUDE_KAN_PMEM_H__
#include <kan/cdefs.h>
#include <kan/page.h>
#include <stddef.h>
#include <stdint.h>

#define ZONE_NORMAL 0x0000
#define ZONE_DMA    0x0001
#define ZONE_DMA32  0x0002
#define ZONE_BIT(x) ((unsigned int)(1 << (x)))

int add_memblock(uintptr_t address, size_t npages, unsigned int zone_bits);
size_t get_total_memory(void);
size_t get_used_memory(void);

uintptr_t pmalloc(size_t npages, unsigned short zone);
void pmfree(uintptr_t address, size_t npages);
void *pmalloc_hhdm(size_t npages, unsigned short zone);
void pmfree_hhdm(void *restrict ptr, size_t npages);

#endif /* __INCLUDE_KAN_PMEM_H__ */
