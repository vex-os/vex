/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_PMM_H__
#define __INCLUDE_SYS_PMM_H__
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

uintptr_t pmm_alloc(size_t npages);
void *pmm_alloc_virt(size_t npages);
void pmm_free(uintptr_t address, size_t npages);
void pmm_free_virt(void *restrict ptr, size_t npages);
void pmm_add_block(uintptr_t address, size_t length);
size_t pmm_get_total_memory(void);
size_t pmm_get_used_memory(void);

#endif /* __INCLUDE_SYS_PMM_H__ */
