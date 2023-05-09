/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <iecprefix.h>
#include <kan/errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/heap.h>
#include <sys/page.h>
#include <sys/pmem.h>
#include <sys/systm.h>
#include <sys/vmm.h>

vma_t *kernel_vma = NULL;

static pml_t *pml_lookup(pml_t *restrict table, size_t index, bool allocate)
{
    uintptr_t entry;
    pml_t *entry_highmem;

    if((table[index] & VMM_VALID)) {
        /* Table entries with the VMM_VALID bit
         * set are assumed to have a valid physical address */
        return (pml_t *)(VMM_ADDR(table[index]) + highmem_offset);
    }

    if(allocate) {
        if((entry = pmem_alloc(1)) != 0) {
            entry_highmem = (pml_t *)(entry + highmem_offset);
            table[index] = entry | VMM_VALID | VMM_WRIT | VMM_USER;
            memset(entry_highmem, 0, PAGE_SIZE);
            return entry_highmem;
        }
    }

    return NULL;
}

static void pml_collapse(pml_t *restrict table, size_t begin, size_t end, unsigned int level)
{
    size_t i;
    pml_t *next;

    if(level != 0) {
        for(i = begin; i < end; ++i) {
            if((next = pml_lookup(table, i, false)) == NULL)
                continue;
            pml_collapse(next, 0, NUM_PML_ENTRIES, (level - 1));
        }

        pmem_free_highmem(table, 1);
    }
}

static pml_t *pml_lookup_pte(pml_t *restrict table, uintptr_t virt, bool allocate)
{
    if((bootflags & BOOTFLAG_VMM_PML5) && ((table = pml_lookup(table, ((virt & (VMM_PML_MASK << VMM_PML5_SHIFT)) >> VMM_PML5_SHIFT), allocate)) == NULL))
        return NULL;
    if((bootflags & BOOTFLAG_VMM_PML4) && ((table = pml_lookup(table, ((virt & (VMM_PML_MASK << VMM_PML4_SHIFT)) >> VMM_PML4_SHIFT), allocate)) == NULL))
        return NULL;
    if((bootflags & BOOTFLAG_VMM_PML3) && ((table = pml_lookup(table, ((virt & (VMM_PML_MASK << VMM_PML3_SHIFT)) >> VMM_PML3_SHIFT), allocate)) == NULL))
        return NULL;
    if((table = pml_lookup(table, ((virt & (VMM_PML_MASK << VMM_PML2_SHIFT)) >> VMM_PML2_SHIFT), allocate)) == NULL)
        return NULL;
    return &table[((virt & (VMM_PML_MASK << VMM_PML1_SHIFT)) >> VMM_PML1_SHIFT)];
}

vma_t *vmm_create(void)
{
    size_t i;
    vma_t *vma;

    if((vma = malloc(sizeof(vma_t))) != NULL) {
        if((vma->table = pmem_alloc_highmem(1)) != NULL) {
            memset(vma->table, 0, PAGE_SIZE);

            for(i = PML_HIGHER_HALF; i < NUM_PML_ENTRIES; ++i) {
                /* This should work for now but if
                 * Kan's kernel ever gets any traction
                 * there should be something that fixes Meltdown */
                vma->table[i] = kernel_vma->table[i];
            }

            return vma;
        }

        free(vma);
    }

    return NULL;
}

vma_t *vmm_fork(vma_t *restrict vma)
{
    /* Implement me */
    return NULL;
}

void vmm_destroy(vma_t *restrict vma)
{
    unsigned int level = 2;
    if(bootflags & BOOTFLAG_VMM_PML3)
        level = 3;
    if(bootflags & BOOTFLAG_VMM_PML4)
        level = 4;
    if(bootflags & BOOTFLAG_VMM_PML5)
        level = 5;
    pml_collapse(vma->table, 0, PML_HIGHER_HALF, level);
}

void vmm_switch_to(vma_t *restrict vma)
{
    load_pagemap((uintptr_t)vma->table - highmem_offset);
}

int vmm_map(vma_t *restrict vma, uintptr_t virt, uintptr_t phys, vmode_t mode)
{
    pml_t *entry;

    /* Ensure page alignment */
    virt = page_align_address(virt);
    phys = page_align_address(phys);

    if((entry = pml_lookup_pte(vma->table, virt, true)) != NULL) {
        if(!(entry[0] & VMM_VALID)) {
            entry[0] = VMM_ADDR(phys) | VMM_VALID | VMM_MODE(mode);
            invalidate_tlb(virt);
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

int vmm_mode(vma_t *restrict vma, uintptr_t virt, vmode_t mode)
{
    pml_t *entry;

    /* Ensure page alignment */
    virt = page_align_address(virt);

    if((entry = pml_lookup_pte(vma->table, virt, false)) != NULL) {
        if((entry[0] & VMM_VALID)) {
            entry[0] = VMM_ADDR(entry[0]) | VMM_VALID | VMM_MODE(mode);
            invalidate_tlb(virt);
            return 0;
        }
    }

    return EINVAL;
}

int vmm_unmap(vma_t *restrict vma, uintptr_t virt)
{
    pml_t *entry;

    /* Ensure page alignment */
    virt = page_align_address(virt);

    if((entry = pml_lookup_pte(vma->table, virt, false)) != NULL) {
        if((entry[0] & VMM_VALID)) {
            entry[0] = 0;
            invalidate_tlb(virt);
            return 0;
        }
    }

    return EINVAL;
}

static int vmm_map_range(vma_t *restrict vma, uintptr_t vstart, uintptr_t vend, uintptr_t phys, vmode_t mode)
{
    int r;
    uintptr_t virt;

    /* Ensure page alignment */
    vstart = page_align_address(vstart);
    vend = __align_ceil(vend, PAGE_SIZE);
    phys = page_align_address(phys);

    virt = vstart;
    while(virt < vend) {
        if((r = vmm_map(vma, virt, phys, mode)) != 0)
            return r;
        virt += PAGE_SIZE;
        phys += PAGE_SIZE;
    }

    return 0;
}

static void init_vmm(void)
{
    size_t i;
    uintptr_t phys;
    uintptr_t vstart;
    uintptr_t vend;

    kernel_vma = malloc(sizeof(vma_t));
    kassertf(kernel_vma, "vmm: insufficient memory");

    kernel_vma->table = pmem_alloc_highmem(1);
    kassertf(kernel_vma->table, "vmm: insufficient memory");

    memset(kernel_vma->table, 0, PAGE_SIZE);

    for(i = PML_HIGHER_HALF; i < NUM_PML_ENTRIES; ++i) {
        if(!pml_lookup(kernel_vma->table, i, true)) {
            panic("vmm: insufficient memory");
            UNREACHABLE();
        }
    }

    vstart = page_align_address((uintptr_t)text_start);
    vend = __align_ceil((uintptr_t)text_end, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!vmm_map_range(kernel_vma, vstart, vend, phys, 0));

    vstart = page_align_address((uintptr_t)rodata_start);
    vend = __align_ceil((uintptr_t)rodata_end, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!vmm_map_range(kernel_vma, vstart, vend, phys, VMM_NOEX));

    vstart = page_align_address((uintptr_t)data_start);
    vend = __align_ceil((uintptr_t)data_end, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!vmm_map_range(kernel_vma, vstart, vend, phys, VMM_WRIT | VMM_NOEX));

    vstart = page_align_address((uintptr_t)bss_start);
    vend = __align_ceil((uintptr_t)bss_end, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!vmm_map_range(kernel_vma, vstart, vend, phys, VMM_WRIT | VMM_NOEX));

    kassert(!vmm_map_range(kernel_vma, PAGE_SIZE, GiB(4), PAGE_SIZE, VMM_WRIT));
    kassert(!vmm_map_range(kernel_vma, highmem_offset + PAGE_SIZE, highmem_offset + GiB(4), PAGE_SIZE, VMM_WRIT | VMM_NOEX));

    /* FIXME: per-fucking-formance goddammit!
     * The more memory machine has, the slower
     * it boots up since now it just maps whatever
     * valid memmap entry it can. The general FIXME idea
     * is to vmm_map MEMMAP_USABLE pages only when they are
     * actually used through pmm_alloc/pmm_alloc_highmem calls */
    for(i = 0; i < memmap_size; ++i) {
        if((memmap[i].type != MEMMAP_RESERVED) && (memmap[i].type != MEMMAP_BAD_MEMORY)) {
            vstart = page_align_address(memmap[i].address);
            vend = __align_ceil(memmap[i].address + memmap[i].length, PAGE_SIZE);

            if(vend > GiB(4)) {
                if(vstart < GiB(4))
                    vstart = GiB(4);
                phys = vstart;
                kassert(!vmm_map_range(kernel_vma, vstart, vend, phys, VMM_WRIT));
                kassert(!vmm_map_range(kernel_vma, vstart + highmem_offset, vend + highmem_offset, phys, VMM_WRIT | VMM_NOEX));
            }
        }
    }

    vmm_switch_to(kernel_vma);

    kprintf("vmm: kernel_vma.table=%p", (void *)((uintptr_t)kernel_vma->table - highmem_offset));
}
early_initcall(vmm, init_vmm);
initcall_dependency(vmm, heap);
