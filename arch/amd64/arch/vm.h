#ifndef B6FB278B_E0A1_44C4_A880_6A58A985FBDC
#define B6FB278B_E0A1_44C4_A880_6A58A985FBDC
#pragma once

#include <core/cdefs.h>
#include <lib/limine.h>
#include <mm/vprot.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AMD64_CR3_ADDRESS_MASK UINT64_C(0x000FFFFFFFFFF000)
#define AMD64_PML_ADDRESS_MASK UINT64_C(0x000FFFFFFFFFF000)

#define AMD64_PML_PRESENT UINT64_C(0x0000000000000001)
#define AMD64_PML_WRITE   UINT64_C(0x0000000000000002)
#define AMD64_PML_USER    UINT64_C(0x0000000000000004)
#define AMD64_PML_NOEXEC  UINT64_C(0x8000000000000000)

#define VM_PAGING_MODE_LVL3 LIMINE_PAGING_MODE_X86_64_4LVL
#define VM_PAGING_MODE_LVL4 LIMINE_PAGING_MODE_X86_64_4LVL
#define VM_PAGING_MODE_LVL5 LIMINE_PAGING_MODE_X86_64_5LVL

#define VM_ENTRY_LVL1_SHIFT UINT64_C(12)
#define VM_ENTRY_LVL2_SHIFT UINT64_C(21)
#define VM_ENTRY_LVL3_SHIFT UINT64_C(30)
#define VM_ENTRY_LVL4_SHIFT UINT64_C(39)
#define VM_ENTRY_LVL5_SHIFT UINT64_C(48)
#define VM_ENTRY_LVLX_MASK  UINT64_C(0x1FF)

#define VM_ENTRY_NULL UINT64_C(0x0000000000000000)

#define VM_KERNEL_BASE UINT64_C(0xFF00000000000000)

#define VM_PAGEMAP_SIZE 512U

typedef uint64_t vm_entry_t;

static __always_inline __nodiscard inline bool vm_entry_valid(vm_entry_t entry)
{
    return (bool)(entry & AMD64_PML_PRESENT);
}

static __always_inline __nodiscard inline uintptr_t vm_entry_address(vm_entry_t entry)
{
    return (uintptr_t)(entry & AMD64_PML_ADDRESS_MASK);
}

static __always_inline __nodiscard inline vm_entry_t vm_entry_init(uintptr_t address, unsigned int vprot)
{
    vm_entry_t result = VM_ENTRY_NULL;

    result |= address & AMD64_PML_ADDRESS_MASK;
    result |= AMD64_PML_PRESENT;
    result |= AMD64_PML_NOEXEC;

    if(vprot & VPROT_WRITE) {
        result |= AMD64_PML_WRITE;
    }

    if(vprot & VPROT_USER) {
        result |= AMD64_PML_USER;
    }

    if(vprot & VPROT_EXEC) {
        result &= ~AMD64_PML_NOEXEC;
    }

    return result;
}

static __always_inline inline void vm_switch_pagemap(uintptr_t phys)
{
    asm volatile("movq %0, %%cr3" : : "r"(phys) : "memory");
    asm volatile("invlpg (%rip)" ::: "memory");
}

#endif /* B6FB278B_E0A1_44C4_A880_6A58A985FBDC */
