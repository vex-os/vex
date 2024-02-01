/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <kern/boot.h>
#include <kern/linker.h>
#include <kern/page.h>
#include <kern/panic.h>
#include <kern/pmm.h>
#include <kern/printf.h>
#include <kern/slab.h>
#include <kern/vmm.h>
#include <machine/cpu.h>
#include <string.h>
#include <sys/vx/errno.h>

/* This assumes the target architecture uses
 * x86-like PML format where a part of the virtual
 * address is an index of a specific level page table.
 * This should work without a fuss on both ARM64 and RISC-V. */
#define PML_INDEX(virt, mask, shift) ((size_t)(((virt) & ((mask) << (shift))) >> (shift)))

/* Kernel's address space */
struct pagemap *sys_vm = NULL;

static vmm_pml_t *pml_get(vmm_pml_t *restrict table, size_t at, bool alloc)
{
    uintptr_t address;
    vmm_pml_t *entry;

    if(!pml_is_valid(table[at])) {
        if(alloc) {
            if((address = pmm_alloc_page()) != 0) {
                table[at] = pml_mkentry(address, VPROT_READ | VPROT_WRITE | VPROT_EXEC | VPROT_USER);
                entry = (vmm_pml_t *)(address + hhdm_offset);
                memset(entry, 0, PAGE_SIZE);
                return entry;
            }
        }

        return NULL;
    }

    return (vmm_pml_t *)(pml_get_address(table[at]) + hhdm_offset);
}

static vmm_pml_t *pml_lookup(vmm_pml_t *restrict table, uintptr_t virt, bool alloc)
{
#if PML_LEVEL5
    if(!(table = pml_get(table, PML_INDEX(virt, PML_L5_MASK, PML_L5_SHIFT), alloc)))
        return NULL;
#endif

#if PML_LEVEL4
    if(!(table = pml_get(table, PML_INDEX(virt, PML_L4_MASK, PML_L4_SHIFT), alloc)))
        return NULL;
#endif

#if PML_LEVEL3
    if(!(table = pml_get(table, PML_INDEX(virt, PML_L3_MASK, PML_L3_SHIFT), alloc)))
        return NULL;
#endif

    if(!(table = pml_get(table, PML_INDEX(virt, PML_L2_MASK, PML_L2_SHIFT), alloc)))
        return NULL;
    return &table[PML_INDEX(virt, PML_L1_MASK, PML_L1_SHIFT)];
}

static void pml_collapse(vmm_pml_t *restrict table, size_t begin, size_t end, unsigned int level)
{
    size_t i;
    vmm_pml_t *next;

    if(level != 0) {
        for(i = begin; i < end; ++i) {
            if(!(next = pml_lookup(table, i, false)))
                continue;
            pml_collapse(next, 0, PML_COUNT, (level - 1));
        }

        pmm_free_page_hhdm(table);
    }
}

struct pagemap *vmm_create(void)
{
    size_t i;
    struct pagemap *vm;

    if((vm = slab_alloc(sizeof(struct pagemap))) != NULL) {
        if((vm->vm_pml_phys = pmm_alloc_page()) != 0) {
            vm->vm_pml = (vmm_pml_t *)(vm->vm_pml_phys + hhdm_offset);
            memset(vm->vm_pml, 0, PAGE_SIZE);

            for(i = PML_KERN; i < PML_COUNT; ++i) {
                /* FIXME: we actually shouldn't let userspace
                 * to see the kernel's address space like that! */
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
    UNREACHABLE();
    return NULL;
}

void vmm_destroy(struct pagemap *restrict vm)
{
#if PML_LEVEL5
    pml_collapse(vm->vm_pml, 0, PML_KERN, 5);
#elif PML_LEVEL4
    pml_collapse(vm->vm_pml, 0, PML_KERN, 4);
#elif PML_LEVEL3
    pml_collapse(vm->vm_pml, 0, PML_KERN, 3);
#else
    pml_collapse(vm->vm_pml, 0, PML_KERN, 2);
#endif
}

void vmm_switch(struct pagemap *restrict vm)
{
    set_cpu_pagemap(vm->vm_pml_phys);
}

int vmm_map(struct pagemap *restrict vm, uintptr_t virt, uintptr_t phys, unsigned short vprot)
{
    vmm_pml_t *entry;

    if((entry = pml_lookup(vm->vm_pml, page_align_address(virt), true)) != NULL) {
        if(!pml_is_valid(entry[0])) {
            entry[0] = pml_mkentry(page_align_address(phys), vprot);
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

int vmm_patch(struct pagemap *restrict vm, uintptr_t virt, unsigned short vprot)
{
    vmm_pml_t *entry;

    if((entry = pml_lookup(vm->vm_pml, page_align_address(virt), false)) != NULL) {
        if(!pml_is_valid(entry[0])) {
            entry[0] = pml_mkentry(pml_get_address(entry[0]), vprot);
            return 0;
        }
    }

    return EINVAL;
}

int vmm_unmap(struct pagemap *restrict vm, uintptr_t virt)
{
    vmm_pml_t *entry;

    if((entry = pml_lookup(vm->vm_pml, page_align_address(virt), false)) != NULL) {
        if(!pml_is_valid(entry[0])) {
            entry[0] = PML_INVALID;
            return 0;
        }
    }

    return EINVAL;
}

static int vmm_map_section(const void *restrict start, const void *restrict end, unsigned short vprot)
{
    int r;
    uintptr_t phys;
    uintptr_t virt;
    uintptr_t virt_end;

    virt = page_align_address((uintptr_t)start);
    virt_end = __align_ceil((uintptr_t)end, PAGE_SIZE);
    phys = virt - kernel_base_virt + kernel_base_phys;

    while(virt < virt_end) {
        if((r = vmm_map(sys_vm, virt, phys, vprot)) == 0) {
            phys += PAGE_SIZE;
            virt += PAGE_SIZE;
            continue;
        }

        return r;
    }

    return 0;
}

static int vmm_map_memmap(const struct limine_memmap_entry *restrict entry)
{
    int r;
    uintptr_t phys;
    uintptr_t virt;
    uintptr_t virt_end;

    phys = page_align_address(entry->base);
    virt = page_align_address(entry->base) + hhdm_offset;
    virt_end = __align_ceil(virt + entry->length, PAGE_SIZE);

    while(virt < virt_end) {
        r = vmm_map(sys_vm, virt, phys, VPROT_READ | VPROT_WRITE | VPROT_EXEC);

        if(r == 0 || r == EINVAL) {
            phys += PAGE_SIZE;
            virt += PAGE_SIZE;
            continue;
        }

        return r;
    }

    return 0;
}

static void init_vmm(void)
{
    int r;
    size_t i;

    if((sys_vm = slab_alloc(sizeof(struct pagemap))) != NULL) {
        if((sys_vm->vm_pml_phys = pmm_alloc_page()) != 0) {
            sys_vm->vm_pml = (vmm_pml_t *)(sys_vm->vm_pml_phys + hhdm_offset);
            memset(sys_vm->vm_pml, 0, PAGE_SIZE);

            for(i = PML_KERN; i < PML_COUNT; ++i) {
                if(!pml_get(sys_vm->vm_pml, i, true)) {
                    panic("vmm: out of memory");
                    UNREACHABLE();
                }
            }

            if((r = vmm_map_section(start_text, end_text, VPROT_READ | VPROT_EXEC)) != 0) {
                panic("vmm: map_section[text]: %s", strerror(r));
                UNREACHABLE();
            }

            if((r = vmm_map_section(start_rodata, end_rodata, VPROT_READ)) != 0) {
                panic("vmm: map_section[rodata]: %s", strerror(r));
                UNREACHABLE();
            }

            if((r = vmm_map_section(start_data, end_data, VPROT_READ | VPROT_WRITE)) != 0) {
                panic("vmm: map_section[data]: %s", strerror(r));
                UNREACHABLE();
            }

            if((r = vmm_map_section(start_bss, end_bss, VPROT_READ | VPROT_WRITE)) != 0) {
                panic("vmm: map_section[bss]: %s", strerror(r));
                UNREACHABLE();
            }

            for(i = 0; i < memmap_request.response->entry_count; ++i) {
                if(memmap_request.response->entries[i]->type != LIMINE_MEMMAP_BAD_MEMORY) {
                    if((r = vmm_map_memmap(memmap_request.response->entries[i])) != 0) {
                        panic("vmm: map_memmap[%zu]: %s", i, strerror(r));
                        UNREACHABLE();
                    }
                }
            }
        }

        vmm_switch(sys_vm);

        kprintf(KP_DEBUG, "vmm: sys_vm.vm_pml=%p", (void *)sys_vm->vm_pml);

        return;
    }

    panic("vmm: out of memory");
    UNREACHABLE();
}
core_initcall(vmm, init_vmm);
initcall_depend(vmm, slab);
initcall_depend(vmm, pmm);
