#ifndef A2C71F06_184E_4FD7_8913_D5ED1EB16655
#define A2C71F06_184E_4FD7_8913_D5ED1EB16655
#pragma once

#include <sys/cdefs.h>
#include <sys/spinlock.h>
#include <sys/types.h>
#include <sys/vprot.h>

#define AMD64_PML_ADDRESS_MASK UINT64_C(0x000FFFFFFFFFF000)

#define AMD64_PML_PRESENT UINT64_C(0x0000000000000001)
#define AMD64_PML_WRITE   UINT64_C(0x0000000000000002)
#define AMD64_PML_USER    UINT64_C(0x0000000000000004)
#define AMD64_PML_PCD     UINT64_C(0x0000000000000010)
#define AMD64_PML_COW     UINT64_C(0x0000000000000200) // software bit 9
#define AMD64_PML_NOEXEC  UINT64_C(0x8000000000000000)

#define AMD64_PAGEMAP_KERN 256U

#define PMENTRY_LVL1_SHIFT UINT64_C(12)
#define PMENTRY_LVL2_SHIFT UINT64_C(21)
#define PMENTRY_LVL3_SHIFT UINT64_C(30)
#define PMENTRY_LVL4_SHIFT UINT64_C(39)
#define PMENTRY_LVL5_SHIFT UINT64_C(48)
#define PMENTRY_LEVEL_MASK UINT64_C(0x1FF)

#define PMENTRY_NULL UINT64_C(0x0000000000000000)

#define PAGEMAP_SIZE     512U
#define PAGEMAP_USER_END AMD64_PAGEMAP_KERN

typedef uint64_t pmentry_type;

struct pagemap {
    spinlock_type lock;
    unsigned refcount;
    unsigned pglevels;

    uintptr_t amd64_cr3_phys;
    pmentry_type* amd64_cr3_virt;
};

unsigned vmm_detect_levels(void);

int pagemap_alloc(struct pagemap* restrict vm) __nodiscard;
void pagemap_free(struct pagemap* restrict vm);
void pagemap_zero(struct pagemap* restrict vm);
void pagemap_kcopy(struct pagemap* restrict dest, const struct pagemap* restrict stem);

static __always_inline __nodiscard pmentry_type* pagemap_root(const struct pagemap* restrict vm, uintptr_t virt)
{
    return vm->amd64_cr3_virt;
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
    if(!(entry & AMD64_PML_NOEXEC))
        vprot |= VPROT_EXEC;
    if(entry & AMD64_PML_PCD)
        vprot |= VPROT_NOCACHE;
    if(entry & AMD64_PML_COW)
        vprot |= VPROT__COW;
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
    if(vprot & VPROT_NOCACHE)
        result |= AMD64_PML_PCD;
    if(vprot & VPROT__COW)
        result |= AMD64_PML_COW;

    return result;
}

static __always_inline void vmm_switch(const struct pagemap* restrict vm)
{
    asm volatile("movq %0, %%cr3" ::"r"(vm->amd64_cr3_phys) : "memory");
}

#endif /* A2C71F06_184E_4FD7_8913_D5ED1EB16655 */
