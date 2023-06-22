/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <machine/cpu.h>
#include <mm/align.h>
#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <string.h>
#include <sys/boot.h>
#include <sys/errno.h>
#include <sys/linker.h>
#include <sys/panic.h>
#include <sys/printf.h>

struct pagemap *sys_vm = NULL;

static vmm_pml_t *pml_lookup(vmm_pml_t *restrict table, size_t offset, bool allocate)
{
    uintptr_t entry;
    vmm_pml_t *entry_highmem;

    if(!pml_valid(table[offset])) {
        if(allocate) {
            if((entry = pmm_alloc_page()) != 0) {
                entry_highmem = (vmm_pml_t *)(entry + hhdm_offset);
                table[offset] = pml_mkentry(entry, VPROT_READ | VPROT_WRITE | VPROT_EXEC | VPROT_USER);
                memset(entry_highmem, 0, PAGE_SIZE);
                return entry_highmem;
            }
        }

        return NULL;
    }

    return (vmm_pml_t *)(pml_address(table[offset]) + hhdm_offset);
}

#define PML_INDEX(virt, mask, shift) ((size_t)(((virt) & ((mask) << (shift))) >> (shift)))

static vmm_pml_t *pml_full_lookup(vmm_pml_t *restrict table, uintptr_t virt, bool allocate)
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

static void pml_dealloc(vmm_pml_t *restrict table, size_t begin, size_t end, unsigned int level)
{
    size_t i;
    vmm_pml_t *next;

    if(level != 0) {
        for(i = begin; i < end; ++i) {
            if((next = pml_lookup(table, i, false)) == NULL)
                continue;
            pml_dealloc(next, 0, PML_COUNT, (level - 1));
        }

        pmm_free_page_hhdm(table);
    }
}

struct pagemap *vmm_create(void)
{
    size_t i;
    struct pagemap *vm;

    if((vm = slab_alloc(sizeof(struct pagemap))) != NULL) {
        if((vm->vm_ptr = pmm_alloc(1)) != 0) {
            vm->vm_pml = (vmm_pml_t *)(vm->vm_ptr + hhdm_offset);
            memset(vm->vm_pml, 0, PAGE_SIZE);

            for(i = PML_KERN; i < PML_COUNT; ++i) {
                /* FIXME: trampoline to mititgate Meltdown? */
                vm->vm_pml[i] = sys_vm->vm_pml[i];
            }

            return vm;
        }

        slab_free(vm);
    }

    return NULL;
}

struct pagemap *vmm_fork(struct pagemap *restrict stem)
{
    panic("vmm_fork is not implemented");
    unreachable();
    return NULL;
}

void vmm_destroy(struct pagemap *restrict vm)
{
#if PML_LEVEL5
    pml_dealloc(vm->vm_pml, 0, PML_KERN, 5);
#elif PML_LEVEL4
    pml_dealloc(vm->vm_pml, 0, PML_KERN, 4);
#elif PML_LEVEL3
    pml_dealloc(vm->vm_pml, 0, PML_KERN, 3);
#else
    pml_dealloc(vm->vm_pml, 0, PML_KERN, 2);
#endif
}

void vmm_switch(struct pagemap *restrict vm)
{
    set_cpu_pagemap(vm->vm_ptr);
}

int vmm_map(struct pagemap *restrict vm, uintptr_t virt, uintptr_t phys, unsigned short vprot)
{
    vmm_pml_t *entry;

    virt = page_align_address(virt);
    phys = page_align_address(phys);

    if((entry = pml_full_lookup(vm->vm_pml, virt, true)) != NULL) {
        if(!pml_valid(entry[0])) {
            entry[0] = pml_mkentry(phys, vprot);
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

int vmm_reflag(struct pagemap *restrict vm, uintptr_t virt, unsigned short vprot)
{
    vmm_pml_t *entry;

    virt = page_align_address(virt);

    if((entry = pml_full_lookup(vm->vm_pml, virt, true)) != NULL) {
        if(!pml_valid(entry[0])) {
            entry[0] = pml_mkentry(pml_address(entry[0]), vprot);
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

int vmm_unmap(struct pagemap *restrict vm, uintptr_t virt)
{
    vmm_pml_t *entry;

    virt = page_align_address(virt);

    if((entry = pml_full_lookup(vm->vm_pml, virt, true)) != NULL) {
        if(!pml_valid(entry[0])) {
            entry[0] = PML_INVALID;
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

static int map_range(struct pagemap *restrict vm, uintptr_t vstart, uintptr_t vend, uintptr_t phys, unsigned short vprot)
{
    int r;
    uintptr_t virt;

    vstart = page_align_address(vstart);
    vend = __align_ceil(vend, PAGE_SIZE);
    phys = page_align_address(phys);

    virt = vstart;
    while(virt < vend) {
        if((r = vmm_map(vm, virt, phys, vprot)) != 0)
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
    uintptr_t vstart, vend;
    struct limine_memmap_entry *entry;

    sys_vm = slab_alloc(sizeof(struct pagemap));
    kassertf(sys_vm, "vmm: insufficient memory");

    sys_vm->vm_ptr = pmm_alloc(1);
    kassertf(sys_vm->vm_ptr, "vmm: insufficient memory");

    sys_vm->vm_pml = (vmm_pml_t *)(sys_vm->vm_ptr + hhdm_offset);
    memset(sys_vm->vm_pml, 0, PAGE_SIZE);

    for(i = PML_KERN; i < PML_COUNT; ++i) {
        if(!pml_lookup(sys_vm->vm_pml, i, true)) {
            panic("vmm: insufficient memory");
            unreachable();
        }
    }

    vstart = page_align_address((uintptr_t)start_text);
    vend = __align_ceil((uintptr_t)end_text, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!map_range(sys_vm, vstart, vend, phys, VPROT_READ | VPROT_EXEC));

    vstart = page_align_address((uintptr_t)start_rodata);
    vend = __align_ceil((uintptr_t)end_rodata, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!map_range(sys_vm, vstart, vend, phys, VPROT_READ));

    vstart = page_align_address((uintptr_t)start_data);
    vend = __align_ceil((uintptr_t)end_data, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!map_range(sys_vm, vstart, vend, phys, VPROT_READ | VPROT_WRITE));

    vstart = page_align_address((uintptr_t)start_bss);
    vend = __align_ceil((uintptr_t)end_bss, PAGE_SIZE);
    phys = vstart - kernel_base_virt + kernel_base_phys;
    kassert(!map_range(sys_vm, vstart, vend, phys, VPROT_READ | VPROT_WRITE));

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        entry = memmap_request.response->entries[i];

        if(entry->type != LIMINE_MEMMAP_RESERVED && entry->type != LIMINE_MEMMAP_BAD_MEMORY) {
            vstart = page_align_address(entry->base) + hhdm_offset;
            vend = __align_ceil(vstart + entry->length, PAGE_SIZE);
            phys = page_align_address(entry->base);
            kassert(!map_range(sys_vm, vstart, vend, phys, VPROT_READ | VPROT_WRITE | VPROT_EXEC));
        }
    }

    vmm_switch(sys_vm);

    kprintf("vmm: sys_vm.vm_table=%p", (void *)sys_vm->vm_ptr);
}
core_initcall(vmm, init_vmm);
initcall_depend(vmm, slab);
