/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __SYS_PMEM_H__
#define __SYS_PMEM_H__
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

void pmem_add_memblock(uintptr_t address, size_t npages);
size_t pmem_get_total_memory(void);
size_t pmem_get_used_memory(void);
uintptr_t pmem_alloc(size_t npages);
void *pmem_alloc_highmem(size_t npages);
void pmem_free(uintptr_t address, size_t npages);
void pmem_free_highmem(void *restrict ptr, size_t npages);

#endif /* __SYS_PMEM_H__ */
