#pragma once

#include <vex/compiler.h>
#include <vex/spinlock.h>
#include <vex/types.h>
#include <vex/vprot.h>

#define AMD64_PML_ADDRESS_MASK UINT64_C(0x000FFFFFFFFFF000)

#define AMD64_PML_PRESENT UINT64_C(0x0000000000000001)
#define AMD64_PML_WRITE   UINT64_C(0x0000000000000002)
#define AMD64_PML_USER    UINT64_C(0x0000000000000004)
#define AMD64_PML_PWT     UINT64_C(0x0000000000000008)
#define AMD64_PML_PCD     UINT64_C(0x0000000000000010)
#define AMD64_PML_PAT     UINT64_C(0x0000000000000080)
#define AMD64_PML_NOEXEC  UINT64_C(0x8000000000000000)

#define AMD64_PAGEMAP_START_KERN 256U

#define PMENTRY_LVL1_SHIFT UINT64_C(12)
#define PMENTRY_LVL2_SHIFT UINT64_C(21)
#define PMENTRY_LVL3_SHIFT UINT64_C(30)
#define PMENTRY_LVL4_SHIFT UINT64_C(39)
#define PMENTRY_LVL5_SHIFT UINT64_C(48)
#define PMENTRY_LEVEL_MASK UINT64_C(0x1FF)

#define PMENTRY_NULL UINT64_C(0x0000000000000000)

#define KSTACK_REGION_BASE UINT64_C(0xFFFFFF0000000000)

#define PAGEMAP_SIZE     512U
#define PAGEMAP_USER_END AMD64_PAGEMAP_START_KERN

// PAT entries 0-3 keep their power-on types
// so PWT and PCD alone still mean what firmware
// and the bootloader expect; entry 4 adds write-combining
#define AMD64_PAT_VALUE UINT64_C(0x0007040100070406)

typedef uint64_t pmentry_type;

struct pagemap {
    spinlock_type lock;
    unsigned num_levels;
    pmentry_type* virt;
    uintptr_t phys;
};

unsigned pmap_detect_levels(void);

int arch_pmap_alloc(struct pagemap* restrict pmap) __nodiscard;
void arch_pmap_free(struct pagemap* restrict pmap);
void arch_pmap_zero(struct pagemap* restrict pmap);
void arch_pmap_kcopy(struct pagemap* restrict dest, const struct pagemap* restrict stem);

static __always_inline __nodiscard pmentry_type* pmap_root(const struct pagemap* restrict pmap, uintptr_t virt)
{
    return pmap->virt;
}

static __always_inline __nodiscard uintptr_t pmap_user_end(const struct pagemap* restrict pmap)
{
    return UINT64_C(1) << (PMENTRY_LVL1_SHIFT + 9 * pmap->num_levels - 1);
}

static __always_inline __nodiscard pmentry_type amd64_pat_bits(unsigned vprot)
{
    switch(vprot & VPROT_CACHE_MASK) {
        case VPROT_CACHE_WT:
            return AMD64_PML_PWT;
        case VPROT_CACHE_UC:
            return AMD64_PML_PCD | AMD64_PML_PWT;
        case VPROT_CACHE_WC:
            return AMD64_PML_PAT;
        default:
            return 0;
    }
}

static __always_inline __nodiscard unsigned amd64_pat_vprot(pmentry_type entry)
{
    static const unsigned types[8] = {
        VPROT_CACHE_WB,
        VPROT_CACHE_WT,
        VPROT_CACHE_UC, // UC-, which MTRRs may turn into write-combining
        VPROT_CACHE_UC,
        VPROT_CACHE_WC,
        VPROT_CACHE_WT,
        VPROT_CACHE_UC,
        VPROT_CACHE_UC,
    };

    unsigned index = 0;

    if(entry & AMD64_PML_PWT)
        index |= 1;
    if(entry & AMD64_PML_PCD)
        index |= 2;
    if(entry & AMD64_PML_PAT)
        index |= 4;

    return types[index];
}

static __always_inline __nodiscard bool pmentry_valid(pmentry_type entry)
{
    return entry & AMD64_PML_PRESENT;
}

static __always_inline __nodiscard uintptr_t pmentry_address(pmentry_type entry)
{
    return entry & AMD64_PML_ADDRESS_MASK;
}

static __always_inline __nodiscard unsigned pmentry_vprot(pmentry_type entry)
{
    unsigned vprot = VPROT_READ;
    if(entry & AMD64_PML_WRITE)
        vprot |= VPROT_WRITE;
    if(entry & AMD64_PML_USER)
        vprot |= VPROT_USER;
    if(0 == (entry & AMD64_PML_NOEXEC))
        vprot |= VPROT_EXEC;
    vprot |= amd64_pat_vprot(entry);
    return vprot;
}

static __always_inline __nodiscard pmentry_type pmentry_setup(uintptr_t address, unsigned vprot)
{
    pmentry_type result = PMENTRY_NULL;

    result |= address & AMD64_PML_ADDRESS_MASK;
    result |= AMD64_PML_PRESENT;
    result |= AMD64_PML_NOEXEC;

    if(vprot & VPROT_WRITE)
        result |= AMD64_PML_WRITE;
    if(vprot & VPROT_USER)
        result |= AMD64_PML_USER;
    if(vprot & VPROT_EXEC)
        result &= ~AMD64_PML_NOEXEC;
    result |= amd64_pat_bits(vprot);

    return result;
}

static __always_inline void pmap_switch(const struct pagemap* restrict pmap)
{
    asm volatile("movq %0, %%cr3" ::"r"(pmap->phys) : "memory");
}

static __always_inline void pmap_flush_all(void)
{
    uint64_t cr3;
    asm volatile("movq %%cr3, %0" : "=r"(cr3)::"memory");
    asm volatile("movq %0, %%cr3" ::"r"(cr3) : "memory");
}

static __always_inline void pmap_flush(uintptr_t virt)
{
    asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}
