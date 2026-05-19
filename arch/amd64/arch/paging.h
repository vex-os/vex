#ifndef B6FB278B_E0A1_44C4_A880_6A58A985FBDC
#define B6FB278B_E0A1_44C4_A880_6A58A985FBDC
#pragma once

#include <core/cdefs.h>
#include <mm/vprot.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AMD64_CR3_ADDRESS_MASK UINT64_C(0x000FFFFFFFFFF000)
#define AMD64_PML_ADDRESS_MASK UINT64_C(0x000FFFFFFFFFF000)

#define AMD64_PML_PRESENT UINT64_C(0x0000000000000001)
#define AMD64_PML_WRITE   UINT64_C(0x0000000000000002)
#define AMD64_PML_USER    UINT64_C(0x0000000000000004)
#define AMD64_PML_BIG     UINT64_C(0x0000000000000080)
#define AMD64_PML_NOEXEC  UINT64_C(0x8000000000000000)

#define PMENTRY_LVL1_SHIFT UINT64_C(12)
#define PMENTRY_LVL2_SHIFT UINT64_C(21)
#define PMENTRY_LVL3_SHIFT UINT64_C(30)
#define PMENTRY_LVL4_SHIFT UINT64_C(39)
#define PMENTRY_LVL5_SHIFT UINT64_C(48)
#define PMENTRY_LEVEL_MASK UINT64_C(0x1FF)

#define PAGEMAP_SIZE 512U
#define PAGEMAP_KERN 256U

#define PMENTRY_NULL UINT64_C(0)

typedef uint64_t pmentry_t;

static __always_inline __nodiscard bool pmentry_is_valid(pmentry_t entry)
{
    return (bool)(entry & AMD64_PML_PRESENT);
}

static __always_inline __nodiscard bool pmentry_is_leaf(pmentry_t entry)
{
    return (bool)(entry & AMD64_PML_BIG);
}

static __always_inline __nodiscard uintptr_t pmentry_address(pmentry_t entry)
{
    return (uintptr_t)(entry & AMD64_PML_ADDRESS_MASK);
}

static __always_inline __nodiscard pmentry_t make_pmentry(uintptr_t address, unsigned int vprot)
{
    pmentry_t entry = PMENTRY_NULL;
    entry |= AMD64_PML_ADDRESS_MASK & address;
    entry |= AMD64_PML_PRESENT;
    entry |= AMD64_PML_NOEXEC;

    if(vprot & VPROT_WRITE) {
        entry |= AMD64_PML_WRITE;
    }

    if(vprot & VPROT_USER) {
        entry |= AMD64_PML_USER;
    }

    if(vprot & VPROT_EXEC) {
        entry &= ~AMD64_PML_NOEXEC;
    }

    if(vprot & (VPROT_BIG1 | VPROT_BIG2)) {
        entry |= AMD64_PML_BIG;
    }

    return entry;
}

static __always_inline __nodiscard int pmentry_level(unsigned int vprot)
{
    if(vprot & VPROT_BIG2)
        return 3;
    if(vprot & VPROT_BIG1)
        return 2;
    return 1;
}

static __always_inline __nodiscard size_t pmentry_index(uintptr_t virt, int level)
{
    static const int shifts[] = {
        0,
        PMENTRY_LVL1_SHIFT,
        PMENTRY_LVL2_SHIFT,
        PMENTRY_LVL3_SHIFT,
        PMENTRY_LVL4_SHIFT,
        PMENTRY_LVL5_SHIFT,
    };

    return (size_t)((virt >> shifts[level]) & PMENTRY_LEVEL_MASK);
}

static __always_inline void pagemap_switch(uintptr_t phys)
{
    asm volatile("movq %0, %%cr3" ::"r"(phys) : "memory");
}

// static __always_inline __nodiscard uintptr_t pagemap_read(void)
// {
//     uint64_t cr3_value;
//     asm volatile("movq %%cr3, %0" : "=r"(cr3_value));
//     return (uintptr_t)(cr3_value & AMD64_CR3_ADDRESS_MASK);
// }

static __always_inline void pagemap_flush_virt(uintptr_t virt)
{
    asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

#endif /* B6FB278B_E0A1_44C4_A880_6A58A985FBDC */
