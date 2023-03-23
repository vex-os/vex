/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_MACHINE_PAGE_H__
#define __INCLUDE_MACHINE_PAGE_H__
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

#if defined(__x86_64__)
#include <x86_64/page.h>
#define PAGE_SIZE X86_PAGE_SIZE
#define VMEM_RDWR X86_PTE_RDWR
#define VMEM_USER X86_PTE_USER
#define VMEM_WTHR X86_PTE_WTHR
#define VMEM_NOEX X86_PTE_NOEX
#else
#warning Unknown architecture
#define PAGE_SIZE 0x1000
#define VMEM_RDWR 0x00000000
#define VMEM_USER 0x00000000
#define VMEM_WTHR 0x00000000
#define VMEM_NOEX 0x00000000
#endif

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

#endif/* __INCLUDE_MACHINE_PAGE_H__ */
