#include <core/errno.h>
#include <core/kprintf.h>
#include <core/linker.h>
#include <core/panic.h>
#include <init/initfn.h>
#include <lib/string.h>
#include <mm/hhdm.h>
#include <mm/kbase.h>
#include <mm/memmap.h>
#include <mm/page.h>
#include <mm/pslab.h>
#include <mm/slab.h>
#include <mm/vmm.h>

struct vmm_pagemap sys_pagemap;

static size_t pagemap_index(uintptr_t virt, uintptr_t shift)
{
    return (size_t)((virt >> shift) & VMM_ENTRY_LEVEL_MASK);
}

static vmm_entry_t* vmm_entry_get(vmm_entry_t* restrict table, size_t index, bool allocate)
{
    uintptr_t phys;
    vmm_entry_t* child;

    if(vmm_entry_valid(table[index])) {
        return phys_to_virt(vmm_entry_address(table[index]));
    }

    if(allocate) {
        phys = pslab_alloc();

        if(phys) {
            child = phys_to_virt(phys);
            memset(child, 0, PAGE_SIZE);
            table[index] = vmm_entry_init(phys, VPROT_URWX);
            return child;
        }
    }

    return NULL;
}

static vmm_entry_t* vmm_entry_lookup(struct vmm_pagemap* restrict vm, uintptr_t virt, bool allocate)
{
    size_t index;
    vmm_entry_t* table = vmm_pagemap_root(vm, virt);

    if(vm->levels >= 5) {
        index = pagemap_index(virt, VMM_ENTRY_LVL5_SHIFT);
        table = vmm_entry_get(table, index, allocate);

        if(table == NULL) {
            return NULL;
        }
    }

    if(vm->levels >= 4) {
        index = pagemap_index(virt, VMM_ENTRY_LVL4_SHIFT);
        table = vmm_entry_get(table, index, allocate);

        if(table == NULL) {
            return NULL;
        }
    }

    if(vm->levels >= 3) {
        index = pagemap_index(virt, VMM_ENTRY_LVL3_SHIFT);
        table = vmm_entry_get(table, index, allocate);

        if(table == NULL) {
            return NULL;
        }
    }

    index = pagemap_index(virt, VMM_ENTRY_LVL2_SHIFT);
    table = vmm_entry_get(table, index, allocate);

    if(table == NULL) {
        return NULL;
    }

    index = pagemap_index(virt, VMM_ENTRY_LVL1_SHIFT);
    return &table[index];
}

static void vmm_table_collapse(vmm_entry_t* restrict table, size_t begin, size_t end, unsigned int depth)
{
    size_t i;
    vmm_entry_t* child;

    if(depth) {
        for(i = begin; i < end; ++i) {
            if(vmm_entry_valid(table[i])) {
                child = phys_to_virt(vmm_entry_address(table[i]));
                vmm_table_collapse(child, 0, VMM_TABLE_SIZE, depth - 1);
                pslab_free(virt_to_phys(child));
            }
        }
    }
}

struct vmm_pagemap* vmm_create(void)
{
    struct vmm_pagemap* vm;

    vm = slab_alloc(sizeof(struct vmm_pagemap));

    if(vm) {
        vm->levels = sys_pagemap.levels;

        if(vmm_pagemap_alloc(vm) == 0) {
            vmm_pagemap_copy_kernel(vm, &sys_pagemap);
            return vm;
        }

        slab_free(vm);
    }

    return NULL;
}

struct vmm_pagemap* vmm_fork(const struct vmm_pagemap* restrict stem)
{
    panic("vmm_fork is not implemented");
    unreachable();
}

struct vmm_pagemap* vmm_clone(const struct vmm_pagemap* restrict stem)
{
    panic("vmm_clone is not implemented");
    unreachable();
}

void vmm_destroy(struct vmm_pagemap* restrict vm)
{
    vmm_table_collapse(vmm_pagemap_root(vm, 0), 0, VMM_TABLE_SIZE / 2, vm->levels - 1);
    vmm_pagemap_free(vm);
    slab_free(vm);
}

int vmm_map(struct vmm_pagemap* restrict vm, uintptr_t virt, uintptr_t phys, unsigned int vprot)
{
    vmm_entry_t* entry = vmm_entry_lookup(vm, virt, true);

    if(entry == NULL) {
        return ENOMEM;
    }

    if(vmm_entry_valid(entry[0])) {
        return EEXIST;
    }

    entry[0] = vmm_entry_init(page_align(phys), vprot);

    return 0;
}

int vmm_patch(struct vmm_pagemap* restrict vm, uintptr_t virt, unsigned int vprot)
{
    vmm_entry_t* entry = vmm_entry_lookup(vm, virt, false);

    if(entry == NULL) {
        return ENOMEM;
    }

    if(!vmm_entry_valid(*entry)) {
        return EINVAL;
    }

    *entry = vmm_entry_init(vmm_entry_address(*entry), vprot);

    return 0;
}

int vmm_unmap(struct vmm_pagemap* restrict vm, uintptr_t virt)
{
    vmm_entry_t* entry = vmm_entry_lookup(vm, virt, false);

    if(entry == NULL) {
        return ENOMEM;
    }

    if(!vmm_entry_valid(*entry)) {
        return EINVAL;
    }

    *entry = VMM_ENTRY_NULL;

    return 0;
}

uintptr_t vmm_resolve(const struct vmm_pagemap* restrict vm, uintptr_t virt)
{
    vmm_entry_t* entry = vmm_entry_lookup((struct vmm_pagemap*)vm, virt, false);

    if(entry && vmm_entry_valid(*entry)) {
        return vmm_entry_address(*entry) + (virt & (PAGE_SIZE - 1));
    }

    return 0;
}

static int vmm_map_sys_section(const void* restrict start, const void* restrict end, unsigned int vprot)
{
    int retval;
    uintptr_t virt = page_align((uintptr_t)start);
    uintptr_t virt_end = page_align_up((uintptr_t)end);
    uintptr_t phys = virt - kbase_virt + kbase_phys;

    while(virt < virt_end) {
        retval = vmm_map(&sys_pagemap, virt, phys, vprot);

        if(retval == 0) {
            virt += PAGE_SIZE;
            phys += PAGE_SIZE;
            continue;
        }

        return retval;
    }

    return 0;
}

static int vmm_map_sys_memmap(const struct limine_memmap_entry* restrict entry)
{
    int retval;
    uintptr_t phys = page_align(entry->base);
    uintptr_t virt = phys + hhdm_base;
    uintptr_t virt_end = page_align_up(virt + entry->length);

    while(virt < virt_end) {
        retval = vmm_map(&sys_pagemap, virt, phys, VPROT_RWX);

        if(retval == 0 || retval == EEXIST) {
            virt += PAGE_SIZE;
            phys += PAGE_SIZE;
            continue;
        }

        return retval;
    }

    return 0;
}

static int init_vmm(void)
{
    size_t i;
    int retval;

    sys_pagemap.levels = vmm_detect_levels();

    if(vmm_pagemap_alloc(&sys_pagemap)) {
        panic("vmm: out of memory: sys_pagemap");
    }

    vmm_pagemap_zero(&sys_pagemap);

    retval = vmm_map_sys_section(&text_start, &text_end, VPROT_RX);

    if(retval) {
        panic("vmm: failed to map [text]: %s", strerror(retval));
    }

    retval = vmm_map_sys_section(&rodata_start, &rodata_end, VPROT_READ);

    if(retval) {
        panic("vmm: failed to map [rodata]: %s", strerror(retval));
    }

    retval = vmm_map_sys_section(&data_start, &data_end, VPROT_RW);

    if(retval) {
        panic("vmm: failed to map [data]: %s", strerror(retval));
    }

    for(i = 0; i < memmap->entry_count; ++i) {
        retval = vmm_map_sys_memmap(memmap->entries[i]);

        if(retval) {
            panic("vmm: failed to map memmap entry [%zu]: %s", i, strerror(retval));
        }
    }

    vmm_switch(&sys_pagemap);

    kprintf(KP_INFORM, "vmm: sys_pagemap=%p", sys_pagemap.amd64.cr3_virt);

    return 0;
}

DEFINE_INITFN(init_vmm, INITFN_CAT_VMM, INITFN_ORDER_MIDDLE);
