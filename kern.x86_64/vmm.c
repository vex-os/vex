/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <iecprefix.h>
#include <machine/boot.h>
#include <machine/cpu.h>
#include <stdbool.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/klog.h>
#include <sys/link.h>
#include <sys/panic.h>
#include <sys/pmm.h>
#include <sys/slab.h>
#include <sys/vmm.h>

#define PTE_PRESENT 0x0000000000000001
#define PTE_WRITE   0x0000000000000002
#define PTE_USER    0x0000000000000004
#define PTE_NOEXEC  0x8000000000000000

#define PTE_ADDR(x) ((x) & 0x000FFFFFFFFFF000)
#define PTE_MODE(x) ((x) & 0xFFF0000000000FFF)

pagemap_t *sys_pagemap = NULL;

static uint64_t get_pte_mode(unsigned short vprot)
{
    uint64_t flags = PTE_NOEXEC;
    if(vprot & VPROT_WRITE)
        flags |= PTE_WRITE;
    if(vprot & VPROT_EXEC)
        flags &= ~PTE_NOEXEC;
    if(vprot & VPROT_USER)
        flags |= PTE_USER;
    return flags;
}

static uint64_t *pml_lookup(uint64_t *restrict table, size_t index, bool allocate)
{
    uintptr_t entry;
    uint64_t *entry_highmem;

    if((table[index] & PTE_PRESENT)) {
        /* Table entries with the PTE_PRESENT bit
         * set are assumed to have a valid physical address */
        return (uint64_t *)(PTE_ADDR(table[index]) + hhdm_offset);
    }

    if(allocate) {
        if((entry = pmm_alloc(1, PMM_ZONE_NORMAL)) != 0) {
            entry_highmem = (uint64_t *)(entry + hhdm_offset);
            table[index] = entry | PTE_PRESENT | PTE_WRITE | PTE_USER;
            memset(entry_highmem, 0, PAGE_SIZE);
            return entry_highmem;
        }
    }

    return NULL;
}

static void pml_collapse(uint64_t *restrict table, size_t begin, size_t end, unsigned int level)
{
    size_t i;
    uint64_t *next;

    if(level != 0) {
        for(i = begin; i < end; ++i) {
            if((next = pml_lookup(table, i, false)) == NULL)
                continue;
            pml_collapse(next, 0, 512, (level - 1));
        }

        pmm_free_hhdm(table, 1);
    }
}

static uint64_t *pml_lookup_pte(uint64_t *restrict table, uintptr_t virt, bool allocate)
{
    if((table = pml_lookup(table, ((virt & (UINT64_C(0x1FF) << 39)) >> 39), allocate)) == NULL)
        return NULL;
    if((table = pml_lookup(table, ((virt & (UINT64_C(0x1FF) << 30)) >> 30), allocate)) == NULL)
        return NULL;
    if((table = pml_lookup(table, ((virt & (UINT64_C(0x1FF) << 21)) >> 21), allocate)) == NULL)
        return NULL;
    return &table[((virt & (UINT64_C(0x1FF) << 12)) >> 12)];
}

pagemap_t *vmm_alloc(void)
{
    size_t i;
    pagemap_t *pagemap;

    if((pagemap = slab_alloc(sizeof(pagemap_t))) != NULL) {
        if((pagemap->table = pmm_alloc_hhdm(1, PMM_ZONE_NORMAL)) != NULL) {
            memset(pagemap->table, 0, PAGE_SIZE);

            for(i = 256; i < 512; ++i) {
                /* FIXME: this should work for now but if Kan/OS ever
                 * gets any traction there should be something that fixes Meltdown */
                ((uint64_t *)pagemap->table)[i] = ((uint64_t *)sys_pagemap->table)[i];
            }

            return pagemap;
        }

        slab_free(pagemap);
    }

    return NULL;
}

pagemap_t *vmm_fork(pagemap_t *restrict pagemap)
{
    panic("vmm: vmm_fork not implemented");
    UNREACHABLE();
    return NULL;
}

void vmm_free(pagemap_t *restrict pagemap)
{
    pml_collapse(pagemap->table, 0, 256, 4);
    slab_free(pagemap);
}

void vmm_switch(pagemap_t *restrict pagemap)
{
    cpu_write_cr3((uint64_t)pagemap->table - hhdm_offset);
}

int vmm_map(pagemap_t *restrict pagemap, uintptr_t virt, uintptr_t phys, unsigned short vprot)
{
    uint64_t *entry;

    /* Ensure page alignment */
    virt = page_align_address(virt);
    phys = page_align_address(phys);

    if((entry = pml_lookup_pte(pagemap->table, virt, true)) != NULL) {
        if(!(entry[0] & PTE_PRESENT)) {
            entry[0] = PTE_ADDR(phys) | PTE_PRESENT | get_pte_mode(vprot);
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

int vmm_remap(pagemap_t *restrict pagemap, uintptr_t virt, unsigned short vprot)
{
    uint64_t *entry;

    /* Ensure page alignment */
    virt = page_align_address(virt);

    if((entry = pml_lookup_pte(pagemap->table, virt, false)) != NULL) {
        if((entry[0] & PTE_PRESENT)) {
            entry[0] = PTE_ADDR(entry[0]) | PTE_PRESENT | get_pte_mode(vprot);
            return 0;
        }
    }

    return EINVAL;
}

int vmm_unmap(pagemap_t *restrict pagemap, uintptr_t virt)
{
    uint64_t *entry;

    /* Ensure page alignment */
    virt = page_align_address(virt);

    if((entry = pml_lookup_pte(pagemap->table, virt, false)) != NULL) {
        if((entry[0] & PTE_PRESENT)) {
            entry[0] = UINT64_C(0x0000000000000000);
            return 0;
        }
    }

    return EINVAL;
}

/* FIXME: per-fucking-formance goddammit!
 * It just seems to be better if we directly do
 * what vmm_map does because it repeats things internally*/
static int vmm_map_range(pagemap_t *restrict pagemap, uintptr_t vstart, uintptr_t vend, uintptr_t phys, unsigned short mode)
{
    int r;
    uintptr_t virt;

    /* Ensure page alignment */
    vstart = page_align_address(vstart);
    vend = __align_ceil(vend, PAGE_SIZE);
    phys = page_align_address(phys);

    virt = vstart;
    while(virt < vend) {
        if((r = vmm_map(pagemap, virt, phys, mode)) != 0)
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

    sys_pagemap = slab_alloc(sizeof(pagemap_t));
    kassertf(sys_pagemap, "vmm: insufficient memory");

    sys_pagemap->table = pmm_alloc_hhdm(1, PMM_ZONE_NORMAL);
    kassertf(sys_pagemap->table, "vmm: insufficient memory");

    memset(sys_pagemap->table, 0, PAGE_SIZE);

    for(i = 256; i < 512; ++i) {
        if(!pml_lookup(sys_pagemap->table, i, true)) {
            panic("vmm: insufficient memory");
            UNREACHABLE();
        }
    }

    vstart = page_align_address((uintptr_t)text_start);
    vend = __align_ceil((uintptr_t)text_end, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!vmm_map_range(sys_pagemap, vstart, vend, phys, VPROT_READ | VPROT_EXEC));

    vstart = page_align_address((uintptr_t)rodata_start);
    vend = __align_ceil((uintptr_t)rodata_end, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!vmm_map_range(sys_pagemap, vstart, vend, phys, VPROT_READ));

    vstart = page_align_address((uintptr_t)data_start);
    vend = __align_ceil((uintptr_t)data_end, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!vmm_map_range(sys_pagemap, vstart, vend, phys, VPROT_READ | VPROT_WRITE));

    vstart = page_align_address((uintptr_t)bss_start);
    vend = __align_ceil((uintptr_t)bss_end, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!vmm_map_range(sys_pagemap, vstart, vend, phys, VPROT_READ | VPROT_WRITE));

    kassert(!vmm_map_range(sys_pagemap, PAGE_SIZE, GiB(4), PAGE_SIZE, VPROT_READ | VPROT_WRITE | VPROT_EXEC));
    kassert(!vmm_map_range(sys_pagemap, hhdm_offset + PAGE_SIZE, hhdm_offset + GiB(4), PAGE_SIZE, VPROT_READ | VPROT_WRITE));

    /* FIXME: per-fucking-formance goddammit!
     * The more memory machine has, the slower
     * it boots up since now it just maps whatever
     * valid memmap entry it can. The general FIXME idea
     * is to vmm_map MEMMAP_USABLE pages only when they are
     * actually used through pmm_alloc/pmm_alloc_highmem calls */
    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        if(memmap_request.response->entries[i]->type == LIMINE_MEMMAP_RESERVED)
            continue;
        if(memmap_request.response->entries[i]->type == LIMINE_MEMMAP_BAD_MEMORY)
            continue;
        vstart = page_align_address(memmap_request.response->entries[i]->base);
        vend = __align_ceil(memmap_request.response->entries[i]->base + memmap_request.response->entries[i]->length, PAGE_SIZE);

        if(vend > GiB(4)) {
            if(vstart < GiB(4))
                vstart = GiB(4);
            phys = vstart;
            kassert(!vmm_map_range(sys_pagemap, vstart, vend, phys, VPROT_READ | VPROT_WRITE | VPROT_EXEC));
            kassert(!vmm_map_range(sys_pagemap, vstart + hhdm_offset, vend + hhdm_offset, phys, VPROT_READ | VPROT_WRITE));
        }
    }

    vmm_switch(sys_pagemap);

    klog(LOG_INFO, "vmm: sys_pagemap.table=%p", (void *)((uintptr_t)sys_pagemap->table - hhdm_offset));
}
core_initcall(vmm, init_vmm);
initcall_depend(vmm, slab);
