/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_VEX_HHDM_H
#define INCLUDE_VEX_HHDM_H
#include <vex/compiler.h>
#include <vex/limine.h>
#include <stdint.h>

static ALWAYS_INLINE NODISCARD inline void *phys_to_hhdm(uintptr_t address)
{
    return (void *)(address + hhdm_offset);
}

static ALWAYS_INLINE NODISCARD inline uintptr_t hhdm_to_phys(const void *restrict ptr)
{
    return ((uintptr_t)(ptr) - hhdm_offset);
}

#endif /* INCLUDE_VEX_HHDM_H */
