#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

extern uintptr_t hhdm_base;

static __always_inline __nodiscard void* phys_to_virt(uintptr_t physaddr)
{
    return (void*)(physaddr + hhdm_base);
}

static __always_inline __nodiscard uintptr_t virt_to_phys(const void* virtaddr)
{
    return (uintptr_t)virtaddr - hhdm_base;
}
