/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_PMM_H__
#define __INCLUDE_SYS_PMM_H__
#include <stddef.h>
#include <stdint.h>
#include <sys/initcall.h>

uintptr_t pmm_alloc(size_t npages);
void *pmm_alloc_hhdm(size_t npages);
void pmm_free(uintptr_t address, size_t npages);
void pmm_free_hhdm(void *restrict ptr, size_t npages);

#define pmm_alloc_page() pmm_alloc(1)
#define pmm_alloc_page_hhdm() pmm_alloc_hhdm(1)
#define pmm_free_page(address) pmm_free(address, 1)
#define pmm_free_page_hhdm(ptr) pmm_free_hhdm(ptr, 1)

initcall_extern(pmm);

#endif /* __INCLUDE_SYS_PMM_H__ */