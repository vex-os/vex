/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/errno.h>
#include <machine/boot.h>
#include <machine/vmem.h>
#include <stdbool.h>
#include <string.h>
#include <sys/cdefs.h>
#include <sys/debug.h>
#include <sys/kprintf.h>
#include <sys/malloc.h>
#include <sys/pmem.h>

vmem_space_t *sys_space = NULL;

static uint64_t *pml_lookup(uint64_t *restrict pml, size_t index, bool alloc)
{
    uint64_t entry;

    if(pml[index] & X86_VMEM_PRESENT) {
        // X86_VMEM_PRESENT is a magic
        // x86-specific bit marking a table entry as present.
        return (uint64_t *)((pml[index] & X86_ADDR_BITS) + hhdm_offset);
    }

    if(alloc) {
        if((entry = pmem_alloc_page()) != 0) {
            pml[index] = (entry | X86_VMEM_PRESENT);
            return (uint64_t *)(entry + hhdm_offset);
        }
    }    

    return NULL;
}

static void pml_free(uint64_t *restrict pml, size_t a, size_t b, unsigned int level)
{
    size_t i;
    uint64_t *next;

    if(level) {
        for(i = a; i <= b; ++i) {
            if((next = pml_lookup(pml, i, false)) == NULL)
                continue;
            pml_free(next, 0, (X86_PML_SIZE - 1), (level - 1));
        }

        pmem_free_page_virt(pml);
    }
}

static uint64_t *pml_get_pte(uint64_t *restrict pml, uint64_t virt, bool alloc, unsigned int level)
{
    size_t shift = (12 + (9 * (level - 1)));
    size_t index = ((virt & (0x1FF << shift)) >> shift);

    if(level >= 2)
        return pml_get_pte(pml_lookup(pml, index, alloc), virt, alloc, (level - 1));
    return &pml[index];
}

vmem_space_t *vmem_alloc(void)
{
    size_t i;
    uint64_t *vm_dtable;
    uint64_t *vm_stable;
    vmem_space_t *space;

    if((space = malloc(sizeof(vmem_space_t))) != NULL) {
        if((space->vm_table = pmem_alloc_page_virt()) != NULL) {
            memset(space->vm_table, 0, PAGE_SIZE);

            vm_dtable = space->vm_table;
            vm_stable = sys_space->vm_table;

            for(i = X86_PML_HIGH; i < X86_PML_SIZE; i++) {
                // Copy over kernel's mappings
                vm_dtable[i] = vm_stable[i];
            }

            return space;
        }        
    }

    return NULL;
}

void vmem_free(vmem_space_t *restrict space)
{
    // UNDONE: support 5-level paging
    pml_free(space->vm_table, 0, (X86_PML_HIGH - 1), (4));
    free(space);
}

void vmem_switch_to(vmem_space_t *restrict space)
{
    // space->vm_table points to a virtual address since we don't have identity mapping
    asm volatile("movq %0, %%cr3"::"r"((uintptr_t)space - hhdm_offset):"memory");
}

int vmem_map_page(vmem_space_t *restrict space, uintptr_t virt, uintptr_t phys, uintmax_t flags)
{
    uint64_t *pte;

    if((pte = pml_get_pte(space->vm_table, virt, true, (4))) == NULL) {
        // Upper-level table allocation failed miserably
        return EFAULT;
    }

    if(pte[0] & X86_VMEM_PRESENT) {
        // Page is already mapped
        return EINVAL;
    }

    pte[0] = ((phys & X86_ADDR_BITS) | ((flags | X86_VMEM_PRESENT) & X86_FLAG_BITS));

    return 0;
}

int vmem_unmap_page(vmem_space_t *restrict space, uintptr_t virt)
{
    uint64_t *pte;

    if((pte = pml_get_pte(space->vm_table, virt, false, (4))) == NULL) {
        // Upper-level table lookup failed miserably
        return EFAULT;
    }

    if(!(pte[0] & X86_VMEM_PRESENT)) {
        // Page is already unmapped
        return EINVAL;
    }

    // Non-present, nothing
    pte[0] = 0x0000000000000000;

    return 0;
}

uintptr_t vmem_resolve(vmem_space_t *restrict space, uintptr_t virt)
{
    uint64_t *pte;

    if((pte = pml_get_pte(space->vm_table, virt, false, (4))) == NULL) {
        // Upper-level table lookup failed miserably
        return 0;
    }

    if(!(pte[0] & X86_VMEM_PRESENT)) {
        // Page is not present
        return 0;
    }

    return (pte[0] & X86_ADDR_BITS);
}

static void init_vmem(void)
{
    size_t i;

    sys_space = malloc(sizeof(vmem_space_t));
    kassertf(sys_space, "vmem: insufficient memory");

    sys_space->vm_table = pmem_alloc_page_virt();
    kassertf(sys_space->vm_table, "vmem: insufficient memory");

    memset(sys_space->vm_table, 0, PAGE_SIZE);

    for(i = X86_PML_HIGH; i < X86_PML_SIZE; ++i) {
        // Allocate top level higher-half entries
        kassert(pml_lookup(sys_space->vm_table, i, true) != NULL);
    }

    kprintf("vmem: if we are here then things work sorta okay");
}
early_initcall(vmem, init_vmem);
initcall_dependency(vmem, boot);
initcall_dependency(vmem, pmem);
initcall_dependency(vmem, malloc);
