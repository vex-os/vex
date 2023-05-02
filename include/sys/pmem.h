/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_PMEM_H__
#define __INCLUDE_SYS_PMEM_H__
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/initcall.h>

int pmem_add_memblock(uintptr_t address, size_t n);
size_t pmem_get_total_memory(void);
size_t pmem_get_used_memory(void);

uintptr_t pmem_alloc(size_t npages);
void *pmem_alloc_virt(size_t npages);
void pmem_free(uintptr_t address, size_t npages);
void pmem_free_virt(void *restrict ptr, size_t npages);

#define pmem_alloc_page() pmem_alloc(1)
#define pmem_alloc_virt_page() pmem_alloc_virt(1)
#define pmem_free_page(address) pmem_free((address), 1)
#define pmem_free_virt_page(ptr) pmem_free_virt((ptr), 1)

initcall_extern(pmem);

#endif/* __INCLUDE_SYS_PMEM_H__ */
