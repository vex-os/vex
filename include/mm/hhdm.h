// SPDX-License-Identifier: BSD-2-Clause
#ifndef INCLUDE_MM_HHDM_H
#define INCLUDE_MM_HHDM_H
#include <kern/compiler.h>
#include <stdint.h>

extern uintptr_t hhdm_offset;

void init_hhdm(void);

static __always_inline __nodiscard inline void* phys_to_hhdm(uintptr_t address)
{
    return (void*)(address + hhdm_offset);
}

static __always_inline __nodiscard inline uintptr_t hhdm_to_phys(const void* restrict ptr)
{
    return ((uintptr_t)ptr - hhdm_offset);
}

#endif // INCLUDE_MM_HHDM_H
