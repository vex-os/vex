#ifndef C0F46E14_D6A2_41DC_AF85_9332C30F6444
#define C0F46E14_D6A2_41DC_AF85_9332C30F6444
#pragma once

#include <core/cdefs.h>
#include <stdint.h>

extern uintptr_t hhdm_base;

static __always_inline __nodiscard inline void* phys_to_virt(uintptr_t physaddr)
{
    return (void*)(physaddr + hhdm_base);
}

static __always_inline __nodiscard inline uintptr_t virt_to_phys(const void* virtaddr)
{
    return (uintptr_t)virtaddr - hhdm_base;
}

#endif /* C0F46E14_D6A2_41DC_AF85_9332C30F6444 */
