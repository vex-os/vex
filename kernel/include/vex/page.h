#pragma once

#include <vex/compiler.h>
#include <vex/limits.h>
#include <vex/types.h>

static __always_inline __nodiscard uintptr_t page_align(uintptr_t address)
{
    return align_floor(address, PAGE_SIZE);
}

static __always_inline __nodiscard uintptr_t page_align_up(uintptr_t address)
{
    return align_ceil(address, PAGE_SIZE);
}

static __always_inline __nodiscard void* page_align_ptr(void* restrict ptr)
{
    return (void*)(align_floor((uintptr_t)ptr, PAGE_SIZE));
}

static __always_inline __nodiscard const void* page_align_const_ptr(const void* restrict ptr)
{
    return (const void*)(align_floor((uintptr_t)ptr, PAGE_SIZE));
}

static __always_inline __nodiscard size_t page_count(size_t sz)
{
    return align_ceil(sz, PAGE_SIZE) / PAGE_SIZE;
}

static __always_inline __nodiscard size_t page_offset(uintptr_t address)
{
    return address & (PAGE_SIZE - 1);
}
