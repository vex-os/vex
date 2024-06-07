/* SPDX-License-Identifier: GPL-2.0-only */
#include <machine/cpu.h>
#include <string.h>
#include <sys/boot.h>
#include <sys/errno.h>
#include <sys/linker.h>
#include <sys/page.h>
#include <sys/panic.h>
#include <sys/pmm.h>
#include <sys/printf.h>
#include <sys/slab.h>
#include <sys/vmm.h>

/* This assumes the target architecture uses
 * x86-like pmentry_t format where a part of the virtual
 * address is an index of a specific level page table.
 * This should work without a fuss on both ARM64 and RISC-V. */
#define PMENTRY_INDEX(virt, mask, shift) ((size_t)(((virt) & ((mask) << (shift))) >> (shift)))

/* Kernel's address space */
struct pagemap *sys_vm = NULL;

static pmentry_t *get_pmentry(pmentry_t *restrict table, size_t at, int alloc)
{
    uintptr_t address;
    pmentry_t *entry;

    if(!pmentry_valid(table[at])) {
        if(alloc) {
            if((address = pmm_alloc_page()) != 0) {
                table[at] = make_pmentry(address, VPROT_URWX);
                entry = (pmentry_t *)(address + hhdm_offset);
                memset(entry, 0, PAGE_SIZE);
                return entry;
            }
        }

        return NULL;
    }

    return (pmentry_t *)(pmentry_address(table[at]) + hhdm_offset);
}

static pmentry_t *lookup_pmentry(pmentry_t *restrict table, uintptr_t virt, int alloc)
{
#if PMENTRY_LVL5
    if(!(table = get_pmentry(table, PMENTRY_INDEX(virt, PMENTRY_LVL5_MASK, PMENTRY_LVL5_SHIFT), alloc)))
        return NULL;
#endif

#if PMENTRY_LVL4
    if(!(table = get_pmentry(table, PMENTRY_INDEX(virt, PMENTRY_LVL4_MASK, PMENTRY_LVL4_SHIFT), alloc)))
        return NULL;
#endif

#if PMENTRY_LVL3
    if(!(table = get_pmentry(table, PMENTRY_INDEX(virt, PMENTRY_LVL3_MASK, PMENTRY_LVL3_SHIFT), alloc)))
        return NULL;
#endif

    if(!(table = get_pmentry(table, PMENTRY_INDEX(virt, PMENTRY_LVL2_MASK, PMENTRY_LVL2_SHIFT), alloc)))
        return NULL;
    return &table[PMENTRY_INDEX(virt, PMENTRY_LVL1_MASK, PMENTRY_LVL1_SHIFT)];
}

static void pmentry_collapse(pmentry_t *restrict table, size_t begin, size_t end, unsigned level)
{
    size_t i;
    pmentry_t *next;

    if(level != 0) {
        for(i = begin; i < end; ++i) {
            if(!(next = lookup_pmentry(table, i, 0)))
                continue;
            pmentry_collapse(next, 0, PAGEMAP_SIZE, (level - 1));
        }

        pmm_free_page_hhdm(table);
    }
}

struct pagemap *vmm_create(void)
{
    size_t i;
    struct pagemap *vm;

    if((vm = slab_alloc(sizeof(struct pagemap))) != NULL) {
        if((vm->vm_table_phys = pmm_alloc_page()) != 0) {
            vm->vm_table = (pmentry_t *)(vm->vm_table_phys + hhdm_offset);
            memset(vm->vm_table, 0, PAGE_SIZE);

            for(i = PAGEMAP_KERN; i < PAGEMAP_SIZE; ++i) {
                /* FIXME: we actually shouldn't let userspace
                 * to see the kernel's address space like that! */
                vm->vm_table[i] = sys_vm->vm_table[i];
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
#if PMENTRY_LVL5
    pmentry_collapse(vm->vm_table, 0, PAGEMAP_KERN, 5);
#elif PMENTRY_LVL4
    pmentry_collapse(vm->vm_table, 0, PAGEMAP_KERN, 4);
#elif PMENTRY_LVL3
    pmentry_collapse(vm->vm_table, 0, PAGEMAP_KERN, 3);
#else
    pmentry_collapse(vm->vm_table, 0, PAGEMAP_KERN, 2);
#endif
}

void vmm_switch(struct pagemap *restrict vm)
{
    set_cpu_pagemap(vm->vm_table_phys);
}

int vmm_map(struct pagemap *restrict vm, uintptr_t virt, uintptr_t phys, unsigned vprot)
{
    pmentry_t *entry;

    if((entry = lookup_pmentry(vm->vm_table, page_align(virt), 1)) != NULL) {
        if(!pmentry_valid(entry[0])) {
            entry[0] = make_pmentry(page_align(phys), vprot);
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

int vmm_patch(struct pagemap *restrict vm, uintptr_t virt, unsigned vprot)
{
    pmentry_t *entry;

    if((entry = lookup_pmentry(vm->vm_table, page_align(virt), 0)) != NULL) {
        if(!pmentry_valid(entry[0])) {
            entry[0] = make_pmentry(pmentry_address(entry[0]), vprot);
            return 0;
        }
    }

    return EINVAL;
}

int vmm_unmap(struct pagemap *restrict vm, uintptr_t virt)
{
    pmentry_t *entry;

    if((entry = lookup_pmentry(vm->vm_table, page_align(virt), 0)) != NULL) {
        if(!pmentry_valid(entry[0])) {
            entry[0] = PMENTRY_NULL;
            return 0;
        }
    }

    return EINVAL;
}

static int vmm_map_section(const void *restrict start, const void *restrict end, unsigned vprot)
{
    int r;
    uintptr_t phys;
    uintptr_t virt;
    uintptr_t virt_end;

    virt = page_align((uintptr_t)(start));
    virt_end = page_align_up((uintptr_t)(end));
    phys = virt - kernel_address_virt + kernel_address_phys;

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

    phys = page_align(entry->base);
    virt = page_align(entry->base) + hhdm_offset;
    virt_end = page_align_up(virt + entry->length);

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
        if((sys_vm->vm_table_phys = pmm_alloc_page()) != 0) {
            sys_vm->vm_table = (pmentry_t *)(sys_vm->vm_table_phys + hhdm_offset);
            memset(sys_vm->vm_table, 0, PAGE_SIZE);

            for(i = PAGEMAP_KERN; i < PAGEMAP_SIZE; ++i) {
                if(!get_pmentry(sys_vm->vm_table, i, 1)) {
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

        kprintf(KP_DEBUG, "vmm: sys_vm.vm_table=%p", (void *)(sys_vm->vm_table));

        return;
    }

    panic("vmm: out of memory");
    UNREACHABLE();
}
core_initcall(vmm, init_vmm);
initcall_dependency(vmm, slab);
initcall_dependency(vmm, pmm);
