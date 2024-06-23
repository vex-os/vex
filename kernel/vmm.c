/* SPDX-License-Identifier: Zlib */
#include <arch/cpu.h>
#include <arch/pml.h>
#include <vex/errno.h>
#include <vex/hhdm.h>
#include <vex/limine.h>
#include <vex/linker.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pmm.h>
#include <vex/printf.h>
#include <vex/slab.h>
#include <vex/vmm.h>
#include <string.h>

static int pagemap_lvl3 = 0;
static int pagemap_lvl4 = 0;
static int pagemap_lvl5 = 0;

struct pagemap *sys_vm = NULL;

static size_t pmentry_index(uintptr_t virt, uintptr_t mask, uintptr_t shift)
{
    /* This assumes the target architecture uses
     * x86-like pmentry_t format where a part of the virtual
     * address is an index of a specific level page table.
     * This should work without a fuss on both ARM64 and RISC-V. */
    return (size_t)((virt & (mask << shift)) >> shift);
}

static pmentry_t *get_pmentry(pmentry_t *restrict table, size_t index, int allocate)
{
    pmentry_t *entry;
    uintptr_t address;

    if(!pmentry_valid(table[index])) {
        if(allocate) {
            if((address = pmm_alloc()) != 0) {
                table[index] = make_pmentry(address, VPROT_URWX);
                entry = phys_to_hhdm(address);

                memset(entry, 0, PAGE_SIZE);

                return entry;
            }
        }

        return NULL;
    }

    return phys_to_hhdm(pmentry_address(table[index]));
}

static pmentry_t *lookup_pmentry(pmentry_t *restrict table, uintptr_t virt, int allocate)
{
    size_t index;

    if(PREDICT_LVL5(pagemap_lvl5)) {
        index = pmentry_index(virt, PMENTRY_LVL5_MASK, PMENTRY_LVL5_SHIFT);
        table = get_pmentry(table, index, allocate);
        if(!table) return NULL;
    }

    if(PREDICT_LVL4(pagemap_lvl4)) {
        index = pmentry_index(virt, PMENTRY_LVL4_MASK, PMENTRY_LVL4_SHIFT);
        table = get_pmentry(table, index, allocate);
        if(!table) return NULL;
    }

    if(PREDICT_LVL3(pagemap_lvl3)) {
        index = pmentry_index(virt, PMENTRY_LVL3_MASK, PMENTRY_LVL3_SHIFT);
        table = get_pmentry(table, index, allocate);
        if(!table) return NULL;
    }

    index = pmentry_index(virt, PMENTRY_LVL2_MASK, PMENTRY_LVL2_SHIFT);
    table = get_pmentry(table, index, allocate);
    if(!table) return NULL;

    index = pmentry_index(virt, PMENTRY_LVL1_MASK, PMENTRY_LVL1_SHIFT);
    return &table[index];
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

        pmm_free(hhdm_to_phys(table));
    }
}

struct pagemap *vmm_create(void)
{
    size_t i;
    struct pagemap *vm;

    if((vm = slab_alloc(sizeof(struct pagemap))) != NULL) {
        if((vm->vm_table_phys = pmm_alloc()) != 0) {
            vm->vm_table = phys_to_hhdm(vm->vm_table_phys);
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
    if(PREDICT_LVL5(pagemap_lvl5)) {
        pmentry_collapse(vm->vm_table, 0, PAGEMAP_KERN, 5);
        goto cleanup;
    }

    if(PREDICT_LVL4(pagemap_lvl4)) {
        pmentry_collapse(vm->vm_table, 0, PAGEMAP_KERN, 4);
        goto cleanup;
    }

    if(PREDICT_LVL3(pagemap_lvl3)) {
        pmentry_collapse(vm->vm_table, 0, PAGEMAP_KERN, 3);
        goto cleanup;
    }

    pmentry_collapse(vm->vm_table, 0, PAGEMAP_KERN, 2);

cleanup:
    pmm_free(vm->vm_table_phys);
    slab_free(vm);
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

    phys = page_align(entry->base);
    virt = page_align(entry->base) + hhdm_offset;
    virt_end = page_align_up(virt + entry->length);

    while(virt < virt_end) {
        r = vmm_map(sys_vm, virt, phys, VPROT_RWX);

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

    pagemap_lvl3 = 0;
    pagemap_lvl4 = 0;
    pagemap_lvl5 = 0;

    /* Here's the interaction I had with one of Limine
     * developers in the OSDev Discord server regarding
     * going with such method of VMM testing the waters:
     * 
     *   und: Limine protocol abuse <trollface>
     * xvanc: that was intentional
     * xvanc: i didn't think it needed to be documented
     *   und: was it really? protocol docs don't say that
     * xvanc: yeah we rely on it in limine itself
     * 
     * Henceforth I am no longer going to cringe at this piece
     * of code because the bootloader appears to do the exact
     * same thing, just in different style and at a different time. */
    if(paging_mode_request.response->mode >= PAGING_MODE_LVL3)
        pagemap_lvl3 = 1;
    if(paging_mode_request.response->mode >= PAGING_MODE_LVL4)
        pagemap_lvl4 = 1;
    if(paging_mode_request.response->mode >= PAGING_MODE_LVL5)
        pagemap_lvl5 = 1;

    if((sys_vm = slab_alloc(sizeof(struct pagemap))) == NULL) {
        panic("vmm: out of memory");
        UNREACHABLE();
    }

    if((sys_vm->vm_table_phys = pmm_alloc()) == 0) {
        panic("vmm: out of memory");
        UNREACHABLE();
    }

    sys_vm->vm_table = phys_to_hhdm(sys_vm->vm_table_phys);
    memset(sys_vm->vm_table, 0, PAGE_SIZE);

    /* Allocate top-level sys_vm entries */
    for(i = PAGEMAP_KERN; i < PAGEMAP_SIZE; ++i) {
        if(!get_pmentry(sys_vm->vm_table, i, 1)) {
            panic("vmm: out of memory");
            UNREACHABLE();
        }
    }

    if((r = vmm_map_section(text_start, text_end, VPROT_READ | VPROT_EXEC)) != 0) {
        panic("vmm: map_section[text]: %s", strerror(r));
        UNREACHABLE();
    }

    if((r = vmm_map_section(rodata_start, rodata_end, VPROT_READ)) != 0) {
        panic("vmm: map_section[rodata]: %s", strerror(r));
        UNREACHABLE();
    }

    if((r = vmm_map_section(data_start, data_end, VPROT_READ | VPROT_WRITE)) != 0) {
        panic("vmm: map_section[data]: %s", strerror(r));
        UNREACHABLE();
    }

    if((r = vmm_map_section(bss_start, bss_end, VPROT_READ | VPROT_WRITE)) != 0) {
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

    vmm_switch(sys_vm);

    kprintf(KP_DEBUG, "vmm: sys_vm.vm_table=%p", (void *)(sys_vm->vm_table));
    kprintf(KP_INFORM, "vmm: pagemap_lvl3=%s", pagemap_lvl3 ? "true" : "false");
    kprintf(KP_INFORM, "vmm: pagemap_lvl4=%s", pagemap_lvl4 ? "true" : "false");
    kprintf(KP_INFORM, "vmm: pagemap_lvl5=%s", pagemap_lvl5 ? "true" : "false");
}
CORE_INITCALL(vmm, init_vmm);
INITCALL_DEPENDENCY(vmm, slab);
INITCALL_DEPENDENCY(vmm, pmm);
