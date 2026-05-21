// SPDX-License-Identifier: BSD-3-Clause
#ifndef AE9FFA1D_8A1C_4366_9EC8_7085B303971C
#define AE9FFA1D_8A1C_4366_9EC8_7085B303971C
#pragma once

#include <arch/limits.h>
#include <core/cdefs.h>
#include <stddef.h>
#include <stdint.h>

static __always_inline __nodiscard inline uintptr_t page_align(uintptr_t address)
{
    return align_floor(address, PAGE_SIZE);
}

static __always_inline __nodiscard inline uintptr_t page_align_up(uintptr_t address)
{
    return align_ceil(address, PAGE_SIZE);
}

static __always_inline __nodiscard inline void* page_align_ptr(void* restrict ptr)
{
    return (void*)(align_floor((uintptr_t)ptr, PAGE_SIZE));
}

static __always_inline __nodiscard inline const void* page_align_const_ptr(const void* restrict ptr)
{
    return (const void*)(align_floor((uintptr_t)ptr, PAGE_SIZE));
}

static __always_inline __nodiscard inline size_t page_count(size_t sz)
{
    return align_ceil(sz, PAGE_SIZE) / PAGE_SIZE;
}

#endif /* AE9FFA1D_8A1C_4366_9EC8_7085B303971C */
