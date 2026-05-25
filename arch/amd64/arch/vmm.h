// SPDX-License-Identifier: BSD-3-Clause
#ifndef A2C71F06_184E_4FD7_8913_D5ED1EB16655
#define A2C71F06_184E_4FD7_8913_D5ED1EB16655
#pragma once

#include <core/cdefs.h>
#include <mm/vprot.h>
#include <stddef.h>
#include <stdint.h>

#define AMD64_VMM_ADDRESS_MASK UINT64_C(0x000FFFFFFFFFF000)

#define AMD64_VMM_PML_PRESENT UINT64_C(0x0000000000000001)
#define AMD64_VMM_PML_WRITE   UINT64_C(0x0000000000000002)
#define AMD64_VMM_PML_USER    UINT64_C(0x0000000000000004)
#define AMD64_VMM_PML_NOEXEC  UINT64_C(0x8000000000000000)

#define AMD64_VMM_TABLE_KERN 256U

#define VMM_ENTRY_LVL1_SHIFT UINT64_C(12)
#define VMM_ENTRY_LVL2_SHIFT UINT64_C(21)
#define VMM_ENTRY_LVL3_SHIFT UINT64_C(30)
#define VMM_ENTRY_LVL4_SHIFT UINT64_C(39)
#define VMM_ENTRY_LVL5_SHIFT UINT64_C(48)
#define VMM_ENTRY_LEVEL_MASK UINT64_C(0x1FF)

#define VMM_ENTRY_NULL UINT64_C(0x0000000000000000)

#define VMM_TABLE_SIZE     512U
#define VMM_TABLE_USER_END AMD64_VMM_TABLE_KERN

typedef uint64_t vmm_entry_type;

struct vmm_pagemap {
    unsigned int levels;
    struct {
        uintptr_t cr3_phys;
        vmm_entry_type* cr3_virt;
    } amd64;
};

extern unsigned int vmm_detect_levels(void);

extern int vmm_pagemap_alloc(struct vmm_pagemap* restrict vm) __nodiscard;
extern void vmm_pagemap_free(struct vmm_pagemap* restrict vm);
extern void vmm_pagemap_zero(struct vmm_pagemap* restrict vm);
extern void vmm_pagemap_copy_kernel(struct vmm_pagemap* restrict dest, const struct vmm_pagemap* restrict stem);

static __always_inline __nodiscard inline vmm_entry_type* vmm_pagemap_root(const struct vmm_pagemap* restrict vm, uintptr_t virt)
{
    return vm->amd64.cr3_virt;
}

static __always_inline __nodiscard inline bool vmm_entry_valid(vmm_entry_type entry)
{
    return (bool)(entry & AMD64_VMM_PML_PRESENT);
}

static __always_inline __nodiscard inline uintptr_t vmm_entry_address(vmm_entry_type entry)
{
    return (uintptr_t)(entry & AMD64_VMM_ADDRESS_MASK);
}

static __always_inline __nodiscard inline unsigned int vmm_entry_vprot(vmm_entry_type entry)
{
    unsigned int vprot = VPROT_READ;
    if(entry & AMD64_VMM_PML_WRITE)
        vprot |= VPROT_WRITE;
    if(entry & AMD64_VMM_PML_USER)
        vprot |= VPROT_USER;
    if(!(entry & AMD64_VMM_PML_NOEXEC))
        vprot |= VPROT_EXEC;
    return vprot;
}

static __always_inline __nodiscard inline vmm_entry_type vmm_entry_init(uintptr_t address, unsigned int vprot)
{
    vmm_entry_type result = VMM_ENTRY_NULL;

    result |= address & AMD64_VMM_ADDRESS_MASK;
    result |= AMD64_VMM_PML_PRESENT;
    result |= AMD64_VMM_PML_NOEXEC;

    if(vprot & VPROT_WRITE)
        result |= AMD64_VMM_PML_WRITE;
    if(vprot & VPROT_USER)
        result |= AMD64_VMM_PML_USER;
    if(vprot & VPROT_EXEC)
        result &= ~AMD64_VMM_PML_NOEXEC;

    return result;
}

static __always_inline inline void vmm_switch(const struct vmm_pagemap* restrict vm)
{
    asm volatile("movq %0, %%cr3" ::"r"(vm->amd64.cr3_phys) : "memory");
}

#endif /* A2C71F06_184E_4FD7_8913_D5ED1EB16655 */
