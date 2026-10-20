#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

extern uintptr_t hhdm_base;

static __always_inline __nodiscard void* phys_to_virt(uintptr_t phys)
{
    return (void*)(phys + hhdm_base);
}

static __always_inline __nodiscard uintptr_t virt_to_phys(const void* virt)
{
    return (uintptr_t)virt - hhdm_base;
}
