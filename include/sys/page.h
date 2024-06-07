/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_SYS_PAGE_H
#define _INCLUDE_SYS_PAGE_H
#include <machine/limits.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

static __always_inline __nodiscard inline uintptr_t page_align(uintptr_t address)
{
    return ALIGN_FLOOR(address, PAGE_SIZE);
}

static __always_inline __nodiscard inline uintptr_t page_align_up(uintptr_t address)
{
    return ALIGN_CEIL(address, PAGE_SIZE);
}

static __always_inline __nodiscard inline void *page_align_ptr(void *restrict ptr)
{
    return (void *)(ALIGN_FLOOR((uintptr_t)(ptr), PAGE_SIZE));
}

static __always_inline __nodiscard inline const void *page_align_const_ptr(const void *restrict ptr)
{
    return (const void *)(ALIGN_FLOOR((uintptr_t)(ptr), PAGE_SIZE));
}

static __always_inline __nodiscard inline size_t page_count(size_t sz)
{
    return ALIGN_CEIL(sz, PAGE_SIZE) / PAGE_SIZE;
}

#endif /* _INCLUDE_SYS_PAGE_H */
