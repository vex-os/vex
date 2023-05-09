/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __X86_64_PAGING_H__
#define __X86_64_PAGING_H__
#include <stdint.h>

#define VMM_PML5_SHIFT 48
#define VMM_PML4_SHIFT 39
#define VMM_PML3_SHIFT 30
#define VMM_PML2_SHIFT 21
#define VMM_PML1_SHIFT 12
#define VMM_PML_MASK UINT64_C(0x1FF)

#define VMM_VALID 0x0000000000000001
#define VMM_WRIT 0x0000000000000002
#define VMM_NOEX 0x8000000000000000
#define VMM_USER 0x0000000000000004
#define VMM_GLOB 0x0000000000000100

#define VMM_ADDR(x) ((x) & 0x000FFFFFFFFFF000)
#define VMM_MODE(x) ((x) & 0xFFF0000000000FFF)

typedef uint64_t pml_t;
typedef uint64_t vmode_t;

static __always_inline inline void load_pagemap(uintptr_t pagemap)
{
    asm volatile("movq %0, %%cr3"::"r"(pagemap):"memory");
}

static __always_inline inline void invalidate_tlb(uintptr_t virt)
{
    asm volatile("invlpg (%0)"::"r"(virt):"memory");
}

#endif /* __X86_64_PAGING_H__ */
