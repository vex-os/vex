/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kern/boot.h>
#include <kern/pmm.h>
#include <kern/slab.h>
#include <kern/systm.h>
#include <kern/vmm.h>
#include <machine/cpu.h>
#include <stdbool.h>
#include <string.h>

struct vmm_pagemap *sys_pagemap = NULL;

static pml_t *pml_lookup(pml_t *restrict table, size_t offset, bool allocate)
{
    uintptr_t entry;
    pml_t *entry_highmem;

    if(!pml_valid(table[offset])) {
        if(allocate) {
            if((entry = pmm_alloc(1, PMM_ZONE_NORMAL)) != 0) {
                entry_highmem = (pml_t *)(entry + highmem);
                table[offset] = pml_mkentry(entry, VPROT_READ | VPROT_WRITE | VPROT_EXEC | VPROT_USER);
                memset(entry_highmem, 0, PAGE_SIZE);
                return entry_highmem;
            }
        }

        return NULL;
    }

    return (pml_t *)(pml_address(table[offset]) + highmem);
}

#define PML_INDEX(virt, mask, shift) ((size_t)(((virt) & ((mask) << (shift))) >> (shift)))

static pml_t *pml_full_lookup(pml_t *restrict table, uintptr_t virt, bool allocate)
{
#if PML_LEVEL5
    if((table = pml_lookup(table, PML_INDEX(virt, PML_L5_MASK, PML_L5_SHIFT), true)) == NULL)
        return NULL;
#endif

#if PML_LEVEL4
    if((table = pml_lookup(table, PML_INDEX(virt, PML_L4_MASK, PML_L4_SHIFT), true)) == NULL)
        return NULL;
#endif

#if PML_LEVEL3
    if((table = pml_lookup(table, PML_INDEX(virt, PML_L3_MASK, PML_L3_SHIFT), true)) == NULL)
        return NULL;
#endif

    if((table = pml_lookup(table, PML_INDEX(virt, PML_L2_MASK, PML_L2_SHIFT), true)) == NULL)
        return NULL;
    return &table[PML_INDEX(virt, PML_L1_MASK, PML_L1_SHIFT)];
}

static void pml_dealloc(pml_t *restrict table, size_t begin, size_t end, unsigned int level)
{
    size_t i;
    pml_t *next;

    if(level != 0) {
        for(i = begin; i < end; ++i) {
            if((next = pml_lookup(table, i, false)) == NULL)
                continue;
            pml_dealloc(next, 0, PML_COUNT, (level - 1));
        }

        pmm_free_highmem(table, 1);
    }
}

struct vmm_pagemap *vmm_create(void)
{
    size_t i;
    struct vmm_pagemap *pagemap;

    if((pagemap = slab_alloc(sizeof(struct vmm_pagemap))) != NULL) {
        if((pagemap->vm_ptr = pmm_alloc(1, PMM_ZONE_NORMAL)) != 0) {
            pagemap->vm_table = (pml_t *)(pagemap->vm_ptr + highmem);
            memset(pagemap->vm_table, 0, PAGE_SIZE);

            for(i = PML_KERN; i < PML_COUNT; ++i) {
                /* FIXME: trampoline to mititgate Meltdown? */
                pagemap->vm_table[i] = sys_pagemap->vm_table[i];
            }

            return pagemap;
        }

        slab_free(pagemap);
    }

    return NULL;
}

struct vmm_pagemap *vmm_fork(struct vmm_pagemap *restrict stem)
{
    panic("vmm_fork is not implemented");
    unreachable();
    return NULL;
}

void vmm_destroy(struct vmm_pagemap *restrict pagemap)
{
#if PML_LEVEL5
    pml_dealloc(pagemap->vm_table, 0, PML_KERN, 5);
#elif PML_LEVEL4
    pml_dealloc(pagemap->vm_table, 0, PML_KERN, 4);
#elif PML_LEVEL3
    pml_dealloc(pagemap->vm_table, 0, PML_KERN, 3);
#else
    pml_dealloc(pagemap->vm_table, 0, PML_KERN, 2);
#endif
}

void vmm_switch(struct vmm_pagemap *restrict pagemap)
{
    set_cpu_pagemap(pagemap->vm_ptr);
}

int vmm_map_page(struct vmm_pagemap *restrict pagemap, uintptr_t virt, uintptr_t phys, unsigned short vprot)
{
    pml_t *entry;

    virt = page_align_address(virt);
    phys = page_align_address(phys);

    if((entry = pml_full_lookup(pagemap->vm_table, virt, true)) != NULL) {
        if(!pml_valid(entry[0])) {
            entry[0] = pml_mkentry(phys, vprot);
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

int vmm_remap_page(struct vmm_pagemap *restrict pagemap, uintptr_t virt, unsigned short vprot)
{
    pml_t *entry;

    virt = page_align_address(virt);

    if((entry = pml_full_lookup(pagemap->vm_table, virt, true)) != NULL) {
        if(!pml_valid(entry[0])) {
            entry[0] = pml_mkentry(pml_address(entry[0]), vprot);
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

int vmm_unmap_page(struct vmm_pagemap *restrict pagemap, uintptr_t virt)
{

    pml_t *entry;

    virt = page_align_address(virt);

    if((entry = pml_full_lookup(pagemap->vm_table, virt, true)) != NULL) {
        if(!pml_valid(entry[0])) {
            entry[0] = PML_INVALID;
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

static int vmm_map_range(struct vmm_pagemap *restrict pagemap, uintptr_t vstart, uintptr_t vend, uintptr_t phys, unsigned short vprot)
{
    int r;
    uintptr_t virt;

    /* Ensure page alignment */
    vstart = page_align_address(vstart);
    vend = __align_ceil(vend, PAGE_SIZE);
    phys = page_align_address(phys);

    virt = vstart;
    while(virt < vend) {
        if((r = vmm_map_page(pagemap, virt, phys, vprot)) != 0)
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

    sys_pagemap = slab_alloc(sizeof(struct vmm_pagemap));
    kassertf(sys_pagemap, "vmm: insufficient memory");

    sys_pagemap->vm_ptr = pmm_alloc(1, PMM_ZONE_NORMAL);
    kassertf(sys_pagemap->vm_ptr, "vmm: insufficient memory");

    sys_pagemap->vm_table = (pml_t *)(sys_pagemap->vm_ptr + highmem);
    memset(sys_pagemap->vm_table, 0, PAGE_SIZE);

    for(i = PML_KERN; i < PML_COUNT; ++i) {
        if(!pml_lookup(sys_pagemap->vm_table, i, true)) {
            panic("vmm: insufficient memory");
            unreachable();
        }
    }

    vstart = page_align_address((uintptr_t)start_text);
    vend = __align_ceil((uintptr_t)end_text, PAGE_SIZE);
    phys = vstart - kbase_virt + kbase_phys;
    kassert(!vmm_map_range(sys_pagemap, vstart, vend, phys, VPROT_READ | VPROT_EXEC));

    vstart = page_align_address((uintptr_t)start_rodata);
    vend = __align_ceil((uintptr_t)end_rodata, PAGE_SIZE);
    phys = vstart - kbase_virt + kbase_phys;
    kassert(!vmm_map_range(sys_pagemap, vstart, vend, phys, VPROT_READ));

    vstart = page_align_address((uintptr_t)start_data);
    vend = __align_ceil((uintptr_t)end_data, PAGE_SIZE);
    phys = vstart - kbase_virt + kbase_phys;
    kassert(!vmm_map_range(sys_pagemap, vstart, vend, phys, VPROT_READ | VPROT_WRITE));

    vstart = page_align_address((uintptr_t)start_bss);
    vend = __align_ceil((uintptr_t)end_bss, PAGE_SIZE);
    phys = vstart - kbase_virt + kbase_phys;
    kassert(!vmm_map_range(sys_pagemap, vstart, vend, phys, VPROT_READ | VPROT_WRITE));

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        if(memmap_request.response->entries[i]->type == LIMINE_MEMMAP_RESERVED)
            continue;
        if(memmap_request.response->entries[i]->type == LIMINE_MEMMAP_BAD_MEMORY)
            continue;
        vstart = page_align_address(memmap_request.response->entries[i]->base) + highmem;
        vend = __align_ceil(vstart + memmap_request.response->entries[i]->length, PAGE_SIZE);
        phys = page_align_address(memmap_request.response->entries[i]->base);
        kassert(!vmm_map_range(sys_pagemap, vstart, vend, phys, VPROT_READ | VPROT_WRITE | VPROT_EXEC));
    }

    vmm_switch(sys_pagemap);

    kprintf("vmm: sys_pagemap.vm_table=%p", (void *)sys_pagemap->vm_ptr);
}
core_initcall(vmm, init_vmm);
initcall_depend(vmm, slab);
