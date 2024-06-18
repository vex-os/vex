/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_ISERIX_PAGE_H
#define INCLUDE_ISERIX_PAGE_H
#include <arch/param.h>
#include <iserix/compiler.h>
#include <stddef.h>
#include <stdint.h>

static ALWAYS_INLINE NODISCARD inline uintptr_t page_align(uintptr_t address)
{
    return ALIGN_FLOOR(address, PAGE_SIZE);
}

static ALWAYS_INLINE NODISCARD inline uintptr_t page_align_up(uintptr_t address)
{
    return ALIGN_CEIL(address, PAGE_SIZE);
}

static ALWAYS_INLINE NODISCARD inline void *page_align_ptr(void *restrict ptr)
{
    return (void *)(ALIGN_FLOOR((uintptr_t)(ptr), PAGE_SIZE));
}

static ALWAYS_INLINE NODISCARD inline const void *page_align_const_ptr(const void *restrict ptr)
{
    return (const void *)(ALIGN_FLOOR((uintptr_t)(ptr), PAGE_SIZE));
}

static ALWAYS_INLINE NODISCARD inline size_t page_count(size_t sz)
{
    return ALIGN_CEIL(sz, PAGE_SIZE) / PAGE_SIZE;
}

#endif /* INCLUDE_ISERIX_PAGE_H */
