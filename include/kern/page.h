/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#ifndef INCLUDE_KERN_PAGE_H
#define INCLUDE_KERN_PAGE_H
#include <machine/limits.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/vx/cdefs.h>

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

#endif /* INCLUDE_KERN_PAGE_H */
