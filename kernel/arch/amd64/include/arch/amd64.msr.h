#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#define MSR_EFER           UINT32_C(0xC0000080)
#define MSR_STAR           UINT32_C(0xC0000081)
#define MSR_LSTAR          UINT32_C(0xC0000082)
#define MSR_SFMASK         UINT32_C(0xC0000084)
#define MSR_FS_BASE        UINT32_C(0xC0000100)
#define MSR_GS_BASE        UINT32_C(0xC0000101)
#define MSR_KERNEL_GS_BASE UINT32_C(0xC0000102)

static __always_inline __nodiscard uint64_t rdmsr(uint32_t msreg)
{
    uint32_t lo, hi;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msreg));
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

static __always_inline void wrmsr(uint32_t msreg, uint64_t value)
{
    uint32_t lo = value;
    uint32_t hi = value >> 32;
    asm volatile("wrmsr" ::"a"(lo), "d"(hi), "c"(msreg));
}

static __always_inline void wrmsr_hl(uint32_t msreg, uint32_t hi, uint32_t lo)
{
    asm volatile("wrmsr" ::"a"(lo), "d"(hi), "c"(msreg));
}
