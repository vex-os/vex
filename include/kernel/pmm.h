/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KERNEL_PMM_H__
#define __INCLUDE_KERNEL_PMM_H__
#include <kernel/init.h>
#include <stddef.h>
#include <stdint.h>

uintptr_t pmm_alloc(size_t npages);
void *pmm_alloc_highmem(size_t npages);
void pmm_free(uintptr_t address, size_t npages);
void pmm_free_highmem(void *restrict ptr, size_t npages);

initcall_extern(pmm);

#endif /* __INCLUDE_KERNEL_PMM_H__ */
