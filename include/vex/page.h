#ifndef _VEX_PAGE_H
#define _VEX_PAGE_H 1

#include <arch/const.h>

#include <vex/compiler.h>

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

#endif
