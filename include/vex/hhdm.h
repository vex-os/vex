#ifndef _VEX_HHDM_H
#define _VEX_HHDM_H 1

#include <vex/compiler.h>

#include <stdint.h>

extern uintptr_t hhdm_base;

extern void init_hhdm(void);

static __always_inline __nodiscard inline void* phys_to_hhdm(uintptr_t address)
{
    return (void*)(address + hhdm_base);
}

static __always_inline __nodiscard inline uintptr_t hhdm_to_phys(void* restrict ptr)
{
    return ((uintptr_t)ptr - hhdm_base);
}

#endif
