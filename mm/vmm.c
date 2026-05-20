#include <arch/vm.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/linker.h>
#include <core/panic.h>
#include <init/initfn.h>
#include <lib/limine.h>
#include <lib/string.h>
#include <mm/hhdm.h>
#include <mm/kbase.h>
#include <mm/memmap.h>
#include <mm/page.h>
#include <mm/pslab.h>
#include <mm/slab.h>
#include <mm/vmm.h>

static volatile __used __section(".limine_requests") struct limine_paging_mode_request paging_mode = {
    .id = LIMINE_PAGING_MODE_REQUEST_ID,
    .mode = VM_PAGING_MODE_LVL5,
    .revision = 2,
};

struct vmm_pagemap sys_pagemap;

static size_t pagemap_index(uintptr_t virt, uintptr_t shift)
{
    return (size_t)((virt >> shift) & VM_ENTRY_LVLX_MASK);
}

static size_t pagemap_kernel_index(unsigned int level)
{
    uintptr_t shift;

    switch(level) {
        case 5:
            shift = VM_ENTRY_LVL5_SHIFT;
            break;

        case 4:
            shift = VM_ENTRY_LVL4_SHIFT;
            break;

        default:
            shift = VM_ENTRY_LVL3_SHIFT;
            break;
    }

    return pagemap_index(VM_KERNEL_BASE, shift);
}

static vm_entry_t* vm_entry_get(vm_entry_t* restrict table, size_t index, bool allocate)
{
    uintptr_t phys;
    vm_entry_t* child;

    if(vm_entry_valid(table[index])) {
        return phys_to_virt(vm_entry_address(table[index]));
    }

    if(allocate) {
        phys = pslab_alloc();

        if(phys) {
            child = phys_to_virt(phys);
            memset(child, 0, PAGE_SIZE);
            table[index] = vm_entry_init(phys, VPROT_URWX);
            return child;
        }
    }

    return NULL;
}

static vm_entry_t* vm_entry_lookup(vm_entry_t* restrict root, uintptr_t virt, unsigned int levels, bool allocate)
{
    size_t index;
    vm_entry_t* table = root;

    if(levels >= 5) {
        index = pagemap_index(virt, VM_ENTRY_LVL5_SHIFT);
        table = vm_entry_get(table, index, allocate);

        if(table == NULL) {
            return NULL;
        }
    }

    if(levels >= 4) {
        index = pagemap_index(virt, VM_ENTRY_LVL4_SHIFT);
        table = vm_entry_get(table, index, allocate);

        if(table == NULL) {
            return NULL;
        }
    }

    if(levels >= 3) {
        index = pagemap_index(virt, VM_ENTRY_LVL3_SHIFT);
        table = vm_entry_get(table, index, allocate);

        if(table == NULL) {
            return NULL;
        }
    }

    index = pagemap_index(virt, VM_ENTRY_LVL2_SHIFT);
    table = vm_entry_get(table, index, allocate);

    if(table == NULL) {
        return NULL;
    }

    index = pagemap_index(virt, VM_ENTRY_LVL1_SHIFT);
    return &table[index];
}

static void vm_table_collapse(vm_entry_t* restrict table, size_t begin, size_t end, unsigned int depth)
{
    size_t i;
    vm_entry_t* child;

    if(depth) {
        for(i = begin; i < end; ++i) {
            if(vm_entry_valid(table[i])) {
                child = phys_to_virt(vm_entry_address(table[i]));
                vm_table_collapse(child, 0, VM_PAGEMAP_SIZE, depth - 1);
                pslab_free(virt_to_phys(child));
            }
        }
    }
}

struct vmm_pagemap* vmm_create(void)
{
    size_t kern_index;
    struct vmm_pagemap* vm;

    vm = slab_alloc(sizeof(struct vmm_pagemap));

    if(vm) {
        vm->virt_user = pslab_alloc_virt();

        if(vm->virt_user) {
            kern_index = pagemap_kernel_index(sys_pagemap.levels);

            vm->virt_kern = &vm->virt_user[kern_index];
            vm->phys_user = virt_to_phys(vm->virt_user);
            vm->phys_kern = virt_to_phys(vm->virt_kern);
            vm->levels = sys_pagemap.levels;

            memset(vm->virt_user, 0, kern_index * sizeof(vm_entry_t));
            memcpy(vm->virt_kern, sys_pagemap.virt_kern, (VM_PAGEMAP_SIZE - kern_index) * sizeof(vm_entry_t));

            return vm;
        }

        pslab_free_virt(vm->virt_user);
        goto error;
    }

error:
    slab_free(vm);
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
    vm_table_collapse(vm->virt_user, 0, pagemap_kernel_index(vm->levels), vm->levels - 1);
    pslab_free_virt(vm->virt_user);
    slab_free(vm);
}

void vmm_switch(const struct vmm_pagemap* restrict vm)
{
    vm_switch_pagemap(vm->phys_user);
}

int vmm_map(struct vmm_pagemap* restrict vm, uintptr_t virt, uintptr_t phys, unsigned int vprot)
{
    vm_entry_t* entry = vm_entry_lookup(vm->virt_user, virt, vm->levels, true);

    if(entry == NULL) {
        return ENOMEM;
    }

    if(vm_entry_valid(entry[0])) {
        return EEXIST;
    }

    entry[0] = vm_entry_init(page_align(phys), vprot);

    return 0;
}

int vmm_patch(struct vmm_pagemap* restrict vm, uintptr_t virt, unsigned int vprot)
{
    vm_entry_t* entry = vm_entry_lookup(vm->virt_user, virt, vm->levels, false);

    if(entry == NULL) {
        return ENOMEM;
    }

    if(!vm_entry_valid(*entry)) {
        return EINVAL;
    }

    *entry = vm_entry_init(vm_entry_address(*entry), vprot);

    return 0;
}

int vmm_unmap(struct vmm_pagemap* restrict vm, uintptr_t virt)
{
    vm_entry_t* entry = vm_entry_lookup(vm->virt_user, virt, vm->levels, false);

    if(entry == NULL) {
        return ENOMEM;
    }

    if(!vm_entry_valid(*entry)) {
        return EINVAL;
    }

    *entry = VM_ENTRY_NULL;

    return 0;
}

uintptr_t vmm_resolve(const struct vmm_pagemap* restrict vm, uintptr_t virt)
{
    vm_entry_t* entry = vm_entry_lookup(vm->virt_user, virt, vm->levels, false);

    if(entry && vm_entry_valid(*entry)) {
        return vm_entry_address(*entry) + (virt & (PAGE_SIZE - 1));
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
    unsigned int levels;

    if(paging_mode.response == NULL) {
        panic("vmm: bootloader did not negotiate paging mode");
    }

    if(paging_mode.response->mode >= VM_PAGING_MODE_LVL5) {
        levels = 5;
    }
    else if(paging_mode.response->mode >= VM_PAGING_MODE_LVL4) {
        levels = 4;
    }
    else {
        levels = 3;
    }

    sys_pagemap.levels = levels;
    sys_pagemap.virt_user = pslab_alloc_virt();

    if(sys_pagemap.virt_user == NULL) {
        panic("vmm: out of memory: sys_pagemap");
    }

    size_t kern_idx = pagemap_kernel_index(levels);

    sys_pagemap.phys_user = virt_to_phys(sys_pagemap.virt_user);
    sys_pagemap.virt_kern = &sys_pagemap.virt_user[kern_idx];
    sys_pagemap.phys_kern = sys_pagemap.phys_user + kern_idx * sizeof(vm_entry_t);

    memset(sys_pagemap.virt_user, 0, PAGE_SIZE);

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

    kprintf(KP_INFORM, "vmm: sys_pagemap.kern=%p", sys_pagemap.virt_kern);
    kprintf(KP_INFORM, "vmm: sys_pagemap.user=%p", sys_pagemap.virt_user);

    return 0;
}

DEFINE_INITFN(init_vmm, INITFN_CAT_VMM, INITFN_ORDER_MIDDLE);
