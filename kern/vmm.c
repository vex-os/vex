#include <string.h>
#include <sys/atomic.h>
#include <sys/errno.h>
#include <sys/hhdm.h>
#include <sys/initcall.h>
#include <sys/kbase.h>
#include <sys/kprintf.h>
#include <sys/linker.h>
#include <sys/memmap.h>
#include <sys/page.h>
#include <sys/panic.h>
#include <sys/pslab.h>
#include <sys/slab.h>
#include <sys/spinlock.h>
#include <sys/vmm.h>

struct vmm_pagemap sys_pagemap;

static size_t pte_index(uintptr_t virt, uintptr_t shift)
{
    return (virt >> shift) & VMM_ENTRY_LEVEL_MASK;
}

static vmm_entry_type* vmm_entry_get(vmm_entry_type* restrict table, size_t index, bool allocate)
{
    if(vmm_entry_valid(table[index])) {
        return phys_to_virt(vmm_entry_address(table[index]));
    }

    if(allocate) {
        uintptr_t phys = pslab_alloc();

        if(phys) {
            vmm_entry_type* child = phys_to_virt(phys);
            memset(child, 0, PAGE_SIZE);
            table[index] = vmm_entry_init(phys, VPROT_URWX);
            return child;
        }
    }

    return nullptr;
}

static vmm_entry_type* vmm_pte_walk(struct vmm_pagemap* restrict vm, uintptr_t virt, bool allocate)
{
    vmm_entry_type* table = vmm_pagemap_root(vm, virt);

    if(vm->levels >= 5) {
        size_t index = pte_index(virt, VMM_ENTRY_LVL5_SHIFT);
        table = vmm_entry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    if(vm->levels >= 4) {
        size_t index = pte_index(virt, VMM_ENTRY_LVL4_SHIFT);
        table = vmm_entry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    if(vm->levels >= 3) {
        size_t index = pte_index(virt, VMM_ENTRY_LVL3_SHIFT);
        table = vmm_entry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    size_t index = pte_index(virt, VMM_ENTRY_LVL2_SHIFT);
    table = vmm_entry_get(table, index, allocate);

    if(table == nullptr) {
        return nullptr;
    }

    index = pte_index(virt, VMM_ENTRY_LVL1_SHIFT);
    return &table[index];
}

static void vmm_table_free(vmm_entry_type* restrict table, size_t begin, size_t end, unsigned depth)
{
    if(depth) {
        for(size_t i = begin; i < end; ++i) {
            if(vmm_entry_valid(table[i])) {
                vmm_entry_type* child = phys_to_virt(vmm_entry_address(table[i]));
                vmm_table_free(child, 0, VMM_TABLE_SIZE, depth - 1);
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
        atomic_store(&vm->refcount, 1, ATOMIC_RELAXED);

        if(vmm_pagemap_alloc(vm) == 0) {
            vmm_pagemap_copy_kernel(vm, &sys_pagemap);
            return vm;
        }

        slab_free(vm);
    }

    return nullptr;
}

static void vmm_mark_cow(vmm_entry_type* restrict table, size_t begin, size_t end, unsigned depth)
{
    for(size_t i = begin; i < end; ++i) {
        if(!vmm_entry_valid(table[i]))
            continue;

        if(depth) {
            vmm_entry_type* child = phys_to_virt(vmm_entry_address(table[i]));
            vmm_mark_cow(child, 0, VMM_TABLE_SIZE, depth - 1);
        }
        else {
            unsigned vprot = vmm_entry_vprot(table[i]);

            if(vprot & VPROT_WRITE) {
                table[i] = vmm_entry_init(vmm_entry_address(table[i]), (vprot & ~VPROT_WRITE) | VPROT__COW);
            }
        }
    }
}

static int vmm_clone_table(vmm_entry_type* restrict dest, vmm_entry_type* restrict stem, size_t begin, size_t end, unsigned depth)
{
    for(size_t i = begin; i < end; ++i) {
        if(vmm_entry_valid(stem[i])) {
            vmm_entry_type* stem_child = phys_to_virt(vmm_entry_address(stem[i]));
            uintptr_t dest_child_phys = pslab_alloc();

            if(dest_child_phys == 0) {
                return -ENOMEM;
            }

            vmm_entry_type* dest_child = phys_to_virt(dest_child_phys);

            if(depth) {
                memset(dest_child, 0, PAGE_SIZE);

                dest[i] = vmm_entry_init(dest_child_phys, VPROT_URWX);
                int retval = vmm_clone_table(dest_child, stem_child, 0, VMM_TABLE_SIZE, depth - 1);

                if(retval < 0) {
                    return retval;
                }
            }
            else {
                memcpy(dest_child, stem_child, PAGE_SIZE);

                unsigned stem_vprot = vmm_entry_vprot(stem[i]);

                if(stem_vprot & VPROT__COW) {
                    stem_vprot = (stem_vprot & ~VPROT__COW) | VPROT_WRITE;
                }

                dest[i] = vmm_entry_init(dest_child_phys, stem_vprot);
            }
        }
    }

    return 0;
}

struct vmm_pagemap* vmm_fork(struct vmm_pagemap* restrict stem)
{
    spinlock_acquire(&stem->lock);

    vmm_entry_type* table = vmm_pagemap_root(stem, 0);
    vmm_mark_cow(table, 0, VMM_TABLE_USER_END, stem->levels - 1);
    atomic_fetch_add(&stem->refcount, 1, ATOMIC_RELAXED);

    spinlock_release(&stem->lock);

    return stem;
}

struct vmm_pagemap* vmm_clone(const struct vmm_pagemap* restrict stem)
{
    struct vmm_pagemap* vm = vmm_create();

    if(vm) {
        vmm_entry_type* stem_table = vmm_pagemap_root(stem, 0);
        vmm_entry_type* dest_table = vmm_pagemap_root(vm, 0);
        int retval = vmm_clone_table(dest_table, stem_table, 0, VMM_TABLE_USER_END, vm->levels - 1);

        if(retval == 0) {
            return vm;
        }

        vmm_release(vm);
    }

    return nullptr;
}

void vmm_release(struct vmm_pagemap* restrict vm)
{
    unsigned prev = atomic_fetch_sub(&vm->refcount, 1, ATOMIC_ACQ_REL);

    if(prev == 1) {
        vmm_table_free(vmm_pagemap_root(vm, 0), 0, VMM_TABLE_SIZE / 2, vm->levels - 1);
        vmm_pagemap_free(vm);
        slab_free(vm);
    }
}

int vmm_map(struct vmm_pagemap* restrict vm, uintptr_t virt, uintptr_t phys, unsigned vprot)
{
    spinlock_acquire(&vm->lock);

    vmm_entry_type* entry = vmm_pte_walk(vm, virt, true);
    int retval;

    if(entry == nullptr) {
        retval = -ENOMEM;
    }
    else if(vmm_entry_valid(*entry)) {
        retval = -EEXIST;
    }
    else {
        *entry = vmm_entry_init(page_align(phys), vprot);
        retval = 0;
    }

    spinlock_release(&vm->lock);

    return retval;
}

int vmm_patch(struct vmm_pagemap* restrict vm, uintptr_t virt, unsigned vprot)
{
    spinlock_acquire(&vm->lock);

    vmm_entry_type* entry = vmm_pte_walk(vm, virt, false);
    int retval;

    if(entry == nullptr) {
        retval = -ENOMEM;
    }
    else if(!vmm_entry_valid(*entry)) {
        retval = -EINVAL;
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
    spinlock_acquire(&vm->lock);

    vmm_entry_type* entry = vmm_pte_walk(vm, virt, false);
    int retval;

    if(entry == nullptr) {
        retval = -ENOMEM;
    }
    else if(!vmm_entry_valid(*entry)) {
        retval = -EINVAL;
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
    vmm_entry_type* entry = vmm_pte_walk((struct vmm_pagemap*)vm, virt, false);

    if(entry && vmm_entry_valid(*entry)) {
        return vmm_entry_address(*entry) + (virt & (PAGE_SIZE - 1));
    }

    return 0;
}

struct vmm_pagemap* vmm_handle_fault(struct vmm_pagemap* restrict vm, const struct vmm_fault_info* restrict info)
{
    if(info->present && (info->vprot & VPROT_WRITE)) {
        struct vmm_pagemap* clone = vmm_clone(vm);

        if(clone) {
            vmm_switch(clone);
            vmm_release(vm);
            return clone;
        }
    }

    return nullptr;
}

static int vmm_map_sys_section(const void* restrict start, const void* restrict end, unsigned vprot)
{
    uintptr_t virt = page_align((uintptr_t)start);
    uintptr_t virt_end = page_align_up((uintptr_t)end);
    uintptr_t phys = virt - kbase_virt + kbase_phys;

    while(virt < virt_end) {
        int retval = vmm_map(&sys_pagemap, virt, phys, vprot);

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
    uintptr_t phys = page_align(entry->base);
    uintptr_t virt = phys + hhdm_base;
    uintptr_t virt_end = page_align_up(virt + entry->length);

    while(virt < virt_end) {
        int retval = vmm_map(&sys_pagemap, virt, phys, VPROT_RWX);

        if(retval == 0 || retval == -EEXIST) {
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
    sys_pagemap.lock = SPINLOCK_INIT;
    sys_pagemap.levels = vmm_detect_levels();

    int retval = vmm_pagemap_alloc(&sys_pagemap);

    if(retval < 0) {
        panic("vmm: out of memory");
    }

    vmm_pagemap_zero(&sys_pagemap);

    retval = vmm_map_sys_section(&text_start, &text_end, VPROT_RX);

    if(retval < 0) {
        panic("vmm: failed to map kernel.text");
    }

    retval = vmm_map_sys_section(&rodata_start, &rodata_end, VPROT_READ);

    if(retval < 0) {
        panic("vmm: failed to map kernel.rodata");
    }

    retval = vmm_map_sys_section(&data_start, &data_end, VPROT_RW);

    if(retval < 0) {
        panic("vmm: failed to map kernel.data");
    }

    for(size_t i = 0; i < memmap->entry_count; ++i) {
        retval = vmm_map_sys_memmap(memmap->entries[i]);

        if(retval < 0) {
            panic("vmm: failed to map memmap entry");
        }
    }

    vmm_switch(&sys_pagemap);
}
