#ifndef CD328B6D_F879_4EF6_AB5C_B4DADF87F15B
#define CD328B6D_F879_4EF6_AB5C_B4DADF87F15B
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

static __always_inline __nodiscard uint64_t msr_read(uint32_t msreg)
{
    uint32_t lo, hi;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msreg));
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

static __always_inline void msr_write(uint32_t msreg, uint64_t value)
{
    uint32_t lo = value;
    uint32_t hi = value >> 32;
    asm volatile("wrmsr" ::"a"(lo), "d"(hi), "c"(msreg));
}

static __always_inline void msr_write_hl(uint32_t msreg, uint32_t hi, uint32_t lo)
{
    asm volatile("wrmsr" ::"a"(lo), "d"(hi), "c"(msreg));
}

#endif /* CD328B6D_F879_4EF6_AB5C_B4DADF87F15B */
