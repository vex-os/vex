#ifndef AE9FFA1D_8A1C_4366_9EC8_7085B303971C
#define AE9FFA1D_8A1C_4366_9EC8_7085B303971C
#pragma once

#include <sys/cdefs.h>
#include <sys/limits.h>
#include <sys/types.h>

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

#endif /* AE9FFA1D_8A1C_4366_9EC8_7085B303971C */
