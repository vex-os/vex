/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KERNEL_PAGE_H__
#define __INCLUDE_KERNEL_PAGE_H__
#include <kan/cdefs.h>
#include <machine/limits.h>
#include <stddef.h>
#include <stdint.h>

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

#endif /* __INCLUDE_KERNEL_PAGE_H__ */
