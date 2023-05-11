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
#include <x86_64/boot.h>
#include <x86_64/cpu.h>

#define PTE_PRESENT 0x0000000000000001
#define PTE_WRITE   0x0000000000000002
#define PTE_USER    0x0000000000000004
#define PTE_NOEXEC  0x8000000000000000

#define PTE_ADDR(x) ((x) & 0x000FFFFFFFFFF000)
#define PTE_MODE(x) ((x) & 0xFFF0000000000FFF)

pagemap_t *kernel_pagemap = NULL;

static uint64_t get_pte_mode(unsigned short mode)
{
    uint64_t flags = PTE_NOEXEC;
    if(mode & VMM_WRITE)
        flags |= PTE_WRITE;
    if(mode & VMM_EXECUTE)
        flags &= ~PTE_NOEXEC;
    if(mode & VMM_USER)
        flags |= PTE_USER;
    return flags;
}

static uint64_t *pml_lookup(uint64_t *restrict table, size_t index, bool allocate)
{
    uintptr_t entry;
    uint64_t *entry_highmem;

    if((table[index] & PTE_PRESENT)) {
        /* Table entries with the VMM_VALID bit
         * set are assumed to have a valid physical address */
        return (uint64_t *)(PTE_ADDR(table[index]) + highmem_offset);
    }

    if(allocate) {
        if((entry = pmem_alloc(1)) != 0) {
            entry_highmem = (uint64_t *)(entry + highmem_offset);
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

        pmem_free_highmem(table, 1);
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

pagemap_t *vmm_create(void)
{
    size_t i;
    pagemap_t *vm;

    if((vm = malloc(sizeof(pagemap_t))) != NULL) {
        if((vm->table = pmem_alloc_highmem(1)) != NULL) {
            memset(vm->table, 0, PAGE_SIZE);

            for(i = 256; i < 512; ++i) {
                /* This should work for now but if
                 * Kan's kernel ever gets any traction
                 * there should be something that fixes Meltdown */
                ((uint64_t *)vm->table)[i] = ((uint64_t *)kernel_pagemap->table)[i];
            }

            return vm;
        }

        free(vm);
    }

    return NULL;
}

pagemap_t *vmm_fork(pagemap_t *restrict vm)
{
    panic("vmm: vmm_fork not implemented");
    UNREACHABLE();
    return NULL;
}

void vmm_destroy(pagemap_t *restrict vm)
{
    pml_collapse(vm->table, 0, 256, 4);
    free(vm);
}

void vmm_switch_to(pagemap_t *restrict vm)
{
    write_cr3((uint64_t)vm->table - highmem_offset);
}

int vmm_map(pagemap_t *restrict vm, uintptr_t virt, uintptr_t phys, unsigned short mode)
{
    uint64_t *entry;

    /* Ensure page alignment */
    virt = page_align_address(virt);
    phys = page_align_address(phys);

    if((entry = pml_lookup_pte(vm->table, virt, true)) != NULL) {
        if(!(entry[0] & PTE_PRESENT)) {
            entry[0] = PTE_ADDR(phys) | PTE_PRESENT | get_pte_mode(mode);
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

int vmm_unmap(pagemap_t *restrict vm, uintptr_t virt)
{
    uint64_t *entry;

    /* Ensure page alignment */
    virt = page_align_address(virt);

    if((entry = pml_lookup_pte(vm->table, virt, false)) != NULL) {
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
static int vmm_map_range(pagemap_t *restrict vma, uintptr_t vstart, uintptr_t vend, uintptr_t phys, unsigned short mode)
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

    kernel_pagemap = malloc(sizeof(pagemap_t));
    kassertf(kernel_pagemap, "vmm: insufficient memory");

    kernel_pagemap->table = pmem_alloc_highmem(1);
    kassertf(kernel_pagemap->table, "vmm: insufficient memory");

    memset(kernel_pagemap->table, 0, PAGE_SIZE);

    for(i = 256; i < 512; ++i) {
        if(!pml_lookup(kernel_pagemap->table, i, true)) {
            panic("vmm: insufficient memory");
            UNREACHABLE();
        }
    }

    vstart = page_align_address((uintptr_t)text_start);
    vend = __align_ceil((uintptr_t)text_end, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!vmm_map_range(kernel_pagemap, vstart, vend, phys, VMM_READ | VMM_EXECUTE));

    vstart = page_align_address((uintptr_t)rodata_start);
    vend = __align_ceil((uintptr_t)rodata_end, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!vmm_map_range(kernel_pagemap, vstart, vend, phys, VMM_READ));

    vstart = page_align_address((uintptr_t)data_start);
    vend = __align_ceil((uintptr_t)data_end, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!vmm_map_range(kernel_pagemap, vstart, vend, phys, VMM_READ | VMM_WRITE));

    vstart = page_align_address((uintptr_t)bss_start);
    vend = __align_ceil((uintptr_t)bss_end, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!vmm_map_range(kernel_pagemap, vstart, vend, phys, VMM_READ | VMM_WRITE));

    kassert(!vmm_map_range(kernel_pagemap, PAGE_SIZE, GiB(4), PAGE_SIZE, VMM_READ | VMM_WRITE | VMM_EXECUTE));
    kassert(!vmm_map_range(kernel_pagemap, highmem_offset + PAGE_SIZE, highmem_offset + GiB(4), PAGE_SIZE, VMM_READ | VMM_WRITE));

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
            kassert(!vmm_map_range(kernel_pagemap, vstart, vend, phys, VMM_READ | VMM_WRITE | VMM_EXECUTE));
            kassert(!vmm_map_range(kernel_pagemap, vstart + highmem_offset, vend + highmem_offset, phys, VMM_READ | VMM_WRITE));
        }
    }

    vmm_switch_to(kernel_pagemap);

    kprintf("vmm: kernel_pagemap.table=%p", (void *)((uintptr_t)kernel_pagemap->table - highmem_offset));
}
early_initcall(vmm, init_vmm);
initcall_dependency(vmm, heap);
