#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/linker.h>
#include <core/panic.h>
#include <core/spinlock.h>
#include <init/initcall.h>
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

static vmm_entry_type* vmm_entry_get(vmm_entry_type* restrict table, size_t index, bool allocate)
{
    uintptr_t phys;
    vmm_entry_type* child;

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

    return nullptr;
}

static vmm_entry_type* vmm_entry_lookup(struct vmm_pagemap* restrict vm, uintptr_t virt, bool allocate)
{
    size_t index;
    vmm_entry_type* table = vmm_pagemap_root(vm, virt);

    if(vm->levels >= 5) {
        index = pagemap_index(virt, VMM_ENTRY_LVL5_SHIFT);
        table = vmm_entry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    if(vm->levels >= 4) {
        index = pagemap_index(virt, VMM_ENTRY_LVL4_SHIFT);
        table = vmm_entry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    if(vm->levels >= 3) {
        index = pagemap_index(virt, VMM_ENTRY_LVL3_SHIFT);
        table = vmm_entry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    index = pagemap_index(virt, VMM_ENTRY_LVL2_SHIFT);
    table = vmm_entry_get(table, index, allocate);

    if(table == nullptr) {
        return nullptr;
    }

    index = pagemap_index(virt, VMM_ENTRY_LVL1_SHIFT);
    return &table[index];
}

static void vmm_table_collapse(vmm_entry_type* restrict table, size_t begin, size_t end, unsigned depth)
{
    size_t i;
    vmm_entry_type* child;

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
        vm->lock = SPINLOCK_INIT;
        vm->levels = sys_pagemap.levels;

        if(vmm_pagemap_alloc(vm) == 0) {
            vmm_pagemap_copy_kernel(vm, &sys_pagemap);
            return vm;
        }

        slab_free(vm);
    }

    return nullptr;
}

static int vmm_fork_table(vmm_entry_type* restrict dest, vmm_entry_type* restrict stem, size_t begin, size_t end, unsigned depth)
{
    size_t i;
    uintptr_t dest_child_phys;
    vmm_entry_type* dest_child;
    vmm_entry_type* stem_child;
    unsigned stem_vprot;
    int retval;

    for(i = begin; i < end; ++i) {
        if(vmm_entry_valid(stem[i])) {
            stem_child = phys_to_virt(vmm_entry_address(stem[i]));

            if(depth) {
                dest_child_phys = pslab_alloc();

                if(dest_child_phys == 0) {
                    return ENOMEM;
                }

                dest_child = phys_to_virt(dest_child_phys);
                memset(dest_child, 0, PAGE_SIZE);

                dest[i] = vmm_entry_init(dest_child_phys, VPROT_URWX);
                retval = vmm_fork_table(dest_child, stem_child, 0, VMM_TABLE_SIZE, depth - 1);

                if(retval) {
                    pslab_free(dest_child_phys);

                    return retval;
                }
            }
            else {
                stem_vprot = vmm_entry_vprot(stem[i]) & ~VPROT_WRITE; // readonly so it triggers a page-fault
                stem[i] = vmm_entry_init(vmm_entry_address(stem[i]), stem_vprot);
                dest[i] = stem[i];
            }
        }
    }

    return 0;
}

static int vmm_clone_table(vmm_entry_type* restrict dest, vmm_entry_type* restrict stem, size_t begin, size_t end, unsigned depth)
{
    size_t i;
    uintptr_t dest_child_phys;
    vmm_entry_type* dest_child;
    vmm_entry_type* stem_child;
    unsigned stem_vprot;
    int retval;

    for(i = begin; i < end; ++i) {
        if(vmm_entry_valid(stem[i])) {
            stem_child = phys_to_virt(vmm_entry_address(stem[i]));
            dest_child_phys = pslab_alloc();

            if(dest_child_phys == 0) {
                return ENOMEM;
            }

            dest_child = phys_to_virt(dest_child_phys);

            if(depth) {
                memset(dest_child, 0, PAGE_SIZE);

                dest[i] = vmm_entry_init(dest_child_phys, VPROT_URWX);
                retval = vmm_clone_table(dest_child, stem_child, 0, VMM_TABLE_SIZE, depth - 1);

                if(retval) {
                    return retval;
                }
            }
            else {
                memcpy(dest_child, stem_child, PAGE_SIZE);

                stem_vprot = vmm_entry_vprot(stem[i]);
                dest[i] = vmm_entry_init(dest_child_phys, stem_vprot);
            }
        }
    }

    return 0;
}

struct vmm_pagemap* vmm_fork(const struct vmm_pagemap* restrict stem)
{
    struct vmm_pagemap* vm;
    vmm_entry_type* stem_table;
    vmm_entry_type* dest_table;
    int retval;

    vm = vmm_create();

    if(vm) {
        stem_table = vmm_pagemap_root(stem, 0);
        dest_table = vmm_pagemap_root(vm, 0);
        retval = vmm_fork_table(dest_table, stem_table, 0, VMM_TABLE_USER_END, vm->levels - 1);

        if(retval == 0) {
            return vm;
        }

        vmm_destroy(vm);
    }

    return nullptr;
}

struct vmm_pagemap* vmm_clone(const struct vmm_pagemap* restrict stem)
{
    struct vmm_pagemap* vm;
    vmm_entry_type* stem_table;
    vmm_entry_type* dest_table;
    int retval;

    vm = vmm_create();

    if(vm) {
        stem_table = vmm_pagemap_root(stem, 0);
        dest_table = vmm_pagemap_root(vm, 0);
        retval = vmm_clone_table(dest_table, stem_table, 0, VMM_TABLE_USER_END, vm->levels - 1);

        if(retval == 0) {
            return vm;
        }

        vmm_destroy(vm);
    }

    return nullptr;
}

void vmm_destroy(struct vmm_pagemap* restrict vm)
{
    vmm_table_collapse(vmm_pagemap_root(vm, 0), 0, VMM_TABLE_SIZE / 2, vm->levels - 1);
    vmm_pagemap_free(vm);
    slab_free(vm);
}

int vmm_map(struct vmm_pagemap* restrict vm, uintptr_t virt, uintptr_t phys, unsigned vprot)
{
    vmm_entry_type* entry;
    int retval;

    spinlock_acquire(&vm->lock);

    entry = vmm_entry_lookup(vm, virt, true);

    if(entry == nullptr) {
        retval = ENOMEM;
    }
    else if(vmm_entry_valid(entry[0])) {
        retval = EEXIST;
    }
    else {
        entry[0] = vmm_entry_init(page_align(phys), vprot);
        retval = 0;
    }

    spinlock_release(&vm->lock);

    return retval;
}

int vmm_patch(struct vmm_pagemap* restrict vm, uintptr_t virt, unsigned vprot)
{
    vmm_entry_type* entry;
    int retval;

    spinlock_acquire(&vm->lock);

    entry = vmm_entry_lookup(vm, virt, false);

    if(entry == nullptr) {
        retval = ENOMEM;
    }
    else if(!vmm_entry_valid(*entry)) {
        retval = EINVAL;
    }
    else {
        *entry = vmm_entry_init(vmm_entry_address(*entry), vprot);
        retval = 0;
    }

    spinlock_release(&vm->lock);

    return retval;
}

int vmm_unmap(struct vmm_pagemap* restrict vm, uintptr_t virt)
{
    vmm_entry_type* entry;
    int retval;

    spinlock_acquire(&vm->lock);

    entry = vmm_entry_lookup(vm, virt, false);

    if(entry == nullptr) {
        retval = ENOMEM;
    }
    else if(!vmm_entry_valid(*entry)) {
        retval = EINVAL;
    }
    else {
        *entry = VMM_ENTRY_NULL;
        retval = 0;
    }

    spinlock_release(&vm->lock);

    return retval;
}

uintptr_t vmm_resolve(const struct vmm_pagemap* restrict vm, uintptr_t virt)
{
    vmm_entry_type* entry;

    entry = vmm_entry_lookup((struct vmm_pagemap*)vm, virt, false);

    if(entry && vmm_entry_valid(*entry)) {
        return vmm_entry_address(*entry) + (virt & (PAGE_SIZE - 1));
    }

    return 0;
}

static int vmm_map_sys_section(const void* restrict start, const void* restrict end, unsigned vprot)
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

define_initcall(init_vmm, INITCALL_STAGE_BSP_VMM, INITCALL_ORDER_MIDDLE)
{
    size_t i;
    int retval;

    sys_pagemap.lock = SPINLOCK_INIT;
    sys_pagemap.levels = vmm_detect_levels();

    retval = vmm_pagemap_alloc(&sys_pagemap);
    strict_kassert_text(retval == 0, "out of memory");

    vmm_pagemap_zero(&sys_pagemap);

    retval = vmm_map_sys_section(&text_start, &text_end, VPROT_RX);
    strict_kassert_text(retval == 0, "failed to map kernel.text");

    retval = vmm_map_sys_section(&rodata_start, &rodata_end, VPROT_READ);
    strict_kassert_text(retval == 0, "failed to map kernel.rodata");

    retval = vmm_map_sys_section(&data_start, &data_end, VPROT_RW);
    strict_kassert_text(retval == 0, "failed to map kernel.data");

    for(i = 0; i < memmap->entry_count; ++i) {
        retval = vmm_map_sys_memmap(memmap->entries[i]);
        strict_kassert_text(retval == 0, "failed to map memmap entry");
    }

    vmm_switch(&sys_pagemap);
}
