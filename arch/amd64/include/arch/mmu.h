#ifndef A2C71F06_184E_4FD7_8913_D5ED1EB16655
#define A2C71F06_184E_4FD7_8913_D5ED1EB16655
#pragma once

#include <vex/compiler.h>
#include <vex/spinlock.h>
#include <vex/types.h>
#include <vex/vprot.h>

#define AMD64_PML_ADDRESS_MASK UINT64_C(0x000FFFFFFFFFF000)

#define AMD64_PML_PRESENT UINT64_C(0x0000000000000001)
#define AMD64_PML_WRITE   UINT64_C(0x0000000000000002)
#define AMD64_PML_USER    UINT64_C(0x0000000000000004)
#define AMD64_PML_PCD     UINT64_C(0x0000000000000010)
#define AMD64_PML_NOEXEC  UINT64_C(0x8000000000000000)

#define AMD64_MMU_START_KERN 256U

#define MMU_ENTRY_LVL1_SHIFT UINT64_C(12)
#define MMU_ENTRY_LVL2_SHIFT UINT64_C(21)
#define MMU_ENTRY_LVL3_SHIFT UINT64_C(30)
#define MMU_ENTRY_LVL4_SHIFT UINT64_C(39)
#define MMU_ENTRY_LVL5_SHIFT UINT64_C(48)
#define MMU_ENTRY_LEVEL_MASK UINT64_C(0x1FF)

#define MMU_ENTRY_NULL UINT64_C(0x0000000000000000)

#define MMU_CTX_SIZE     512U
#define MMU_CTX_USER_END AMD64_MMU_START_KERN

typedef uint64_t mmu_entry_type;

struct mmu_ctx {
    spinlock_type lock;
    unsigned num_levels;
    mmu_entry_type* virt;
    uintptr_t phys;
};

unsigned mmu_detect_levels(void);

int arch_mmu_alloc(struct mmu_ctx* restrict ctx) __nodiscard;
void arch_mmu_free(struct mmu_ctx* restrict ctx);
void arch_mmu_zero(struct mmu_ctx* restrict ctx);
void arch_mmu_kcopy(struct mmu_ctx* restrict dest, const struct mmu_ctx* restrict stem);

static __always_inline __nodiscard mmu_entry_type* mmu_root(const struct mmu_ctx* restrict ctx, uintptr_t virt)
{
    return ctx->virt;
}

static __always_inline __nodiscard bool mmu_entry_valid(mmu_entry_type entry)
{
    return entry & AMD64_PML_PRESENT;
}

static __always_inline __nodiscard uintptr_t mmu_entry_address(mmu_entry_type entry)
{
    return entry & AMD64_PML_ADDRESS_MASK;
}

static __always_inline __nodiscard unsigned mmu_entry_vprot(mmu_entry_type entry)
{
    unsigned vprot = VPROT_READ;
    if(entry & AMD64_PML_WRITE)
        vprot |= VPROT_WRITE;
    if(entry & AMD64_PML_USER)
        vprot |= VPROT_USER;
    if(!(entry & AMD64_PML_NOEXEC))
        vprot |= VPROT_EXEC;
    if(entry & AMD64_PML_PCD)
        vprot |= VPROT_NOCACHE;
    return vprot;
}

static __always_inline __nodiscard mmu_entry_type mmu_entry_setup(uintptr_t address, unsigned vprot)
{
    mmu_entry_type result = MMU_ENTRY_NULL;

    result |= address & AMD64_PML_ADDRESS_MASK;
    result |= AMD64_PML_PRESENT;
    result |= AMD64_PML_NOEXEC;

    if(vprot & VPROT_WRITE)
        result |= AMD64_PML_WRITE;
    if(vprot & VPROT_USER)
        result |= AMD64_PML_USER;
    if(vprot & VPROT_EXEC)
        result &= ~AMD64_PML_NOEXEC;
    if(vprot & VPROT_NOCACHE)
        result |= AMD64_PML_PCD;

    return result;
}

static __always_inline void mmu_switch_ctx(const struct mmu_ctx* restrict ctx)
{
    asm volatile("movq %0, %%cr3" ::"r"(ctx->phys) : "memory");
}

static __always_inline void mmu_flush_tlb(uintptr_t virt)
{
    asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

#endif /* A2C71F06_184E_4FD7_8913_D5ED1EB16655 */
