#include <vex/atomic.h>
#include <vex/errno.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/kbase.h>
#include <vex/kprintf.h>
#include <vex/linker.h>
#include <vex/memmap.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pslab.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/vmm.h>

struct pagemap sys_pagemap;

static size_t pte_index(uintptr_t virt, uintptr_t shift)
{
    return (virt >> shift) & PMENTRY_LEVEL_MASK;
}

static pmentry_type* vmm_entry_get(pmentry_type* restrict table, size_t index, bool allocate)
{
    if(pmentry_valid(table[index])) {
        return phys_to_virt(pmentry_address(table[index]));
    }

    if(allocate) {
        uintptr_t phys = pslab_alloc_phys();

        if(phys) {
            pmentry_type* child = phys_to_virt(phys);
            memset(child, 0, PAGE_SIZE);
            table[index] = pmentry_setup(phys, VPROT_URWX);
            return child;
        }
    }

    return nullptr;
}

static pmentry_type* vmm_pte_walk(struct pagemap* restrict vm, uintptr_t virt, bool allocate)
{
    pmentry_type* table = pagemap_root(vm, virt);

    if(vm->pglevels >= 5) {
        size_t index = pte_index(virt, PMENTRY_LVL5_SHIFT);
        table = vmm_entry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    if(vm->pglevels >= 4) {
        size_t index = pte_index(virt, PMENTRY_LVL4_SHIFT);
        table = vmm_entry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    if(vm->pglevels >= 3) {
        size_t index = pte_index(virt, PMENTRY_LVL3_SHIFT);
        table = vmm_entry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    size_t index = pte_index(virt, PMENTRY_LVL2_SHIFT);
    table = vmm_entry_get(table, index, allocate);

    if(table == nullptr) {
        return nullptr;
    }

    index = pte_index(virt, PMENTRY_LVL1_SHIFT);
    return &table[index];
}

static void vmm_table_free(pmentry_type* restrict table, size_t begin, size_t end, unsigned depth)
{
    if(depth) {
        for(size_t i = begin; i < end; ++i) {
            if(pmentry_valid(table[i])) {
                pmentry_type* child = phys_to_virt(pmentry_address(table[i]));
                vmm_table_free(child, 0, PAGEMAP_SIZE, depth - 1);
                pslab_free_phys(virt_to_phys(child));
            }
        }
    }
}

struct pagemap* vmm_create(void)
{
    struct pagemap* vm;

    vm = slab_alloc(sizeof(struct pagemap));

    if(vm) {
        vm->lock = SPINLOCK_INIT;
        vm->pglevels = sys_pagemap.pglevels;
        atomic_store(&vm->refcount, 1, ATOMIC_RELAXED);

        if(pagemap_alloc(vm) == 0) {
            pagemap_kcopy(vm, &sys_pagemap);
            return vm;
        }

        slab_free(vm);
    }

    return nullptr;
}

static void vmm_mark_cow(pmentry_type* restrict table, size_t begin, size_t end, unsigned depth)
{
    for(size_t i = begin; i < end; ++i) {
        if(!pmentry_valid(table[i]))
            continue;

        if(depth) {
            pmentry_type* child = phys_to_virt(pmentry_address(table[i]));
            vmm_mark_cow(child, 0, PAGEMAP_SIZE, depth - 1);
        }
        else {
            unsigned vprot = pmentry_vprot(table[i]);

            if(vprot & VPROT_WRITE) {
                table[i] = pmentry_setup(pmentry_address(table[i]), (vprot & ~VPROT_WRITE) | VPROT__COW);
            }
        }
    }
}

static int vmm_clone_table(pmentry_type* restrict dest, pmentry_type* restrict stem, size_t begin, size_t end, unsigned depth)
{
    for(size_t i = begin; i < end; ++i) {
        if(pmentry_valid(stem[i])) {
            pmentry_type* stem_child = phys_to_virt(pmentry_address(stem[i]));
            uintptr_t dest_child_phys = pslab_alloc_phys();

            if(dest_child_phys == 0) {
                return -ENOMEM;
            }

            pmentry_type* dest_child = phys_to_virt(dest_child_phys);

            if(depth) {
                memset(dest_child, 0, PAGE_SIZE);

                dest[i] = pmentry_setup(dest_child_phys, VPROT_URWX);
                int retval = vmm_clone_table(dest_child, stem_child, 0, PAGEMAP_SIZE, depth - 1);

                if(retval < 0) {
                    return retval;
                }
            }
            else {
                memcpy(dest_child, stem_child, PAGE_SIZE);

                unsigned stem_vprot = pmentry_vprot(stem[i]);

                if(stem_vprot & VPROT__COW) {
                    stem_vprot = (stem_vprot & ~VPROT__COW) | VPROT_WRITE;
                }

                dest[i] = pmentry_setup(dest_child_phys, stem_vprot);
            }
        }
    }

    return 0;
}

struct pagemap* vmm_fork(struct pagemap* restrict stem)
{
    bool intr_state = spinlock_acquire_intr(&stem->lock);

    pmentry_type* table = pagemap_root(stem, 0);
    vmm_mark_cow(table, 0, PAGEMAP_USER_END, stem->pglevels - 1);
    atomic_fetch_add(&stem->refcount, 1, ATOMIC_RELAXED);

    spinlock_release_intr(&stem->lock, intr_state);

    return stem;
}

struct pagemap* vmm_clone(const struct pagemap* restrict stem)
{
    struct pagemap* vm = vmm_create();

    if(vm) {
        pmentry_type* stem_table = pagemap_root(stem, 0);
        pmentry_type* dest_table = pagemap_root(vm, 0);
        int retval = vmm_clone_table(dest_table, stem_table, 0, PAGEMAP_USER_END, vm->pglevels - 1);

        if(retval == 0) {
            return vm;
        }

        vmm_release(vm);
    }

    return nullptr;
}

void vmm_release(struct pagemap* restrict vm)
{
    unsigned prev = atomic_fetch_sub(&vm->refcount, 1, ATOMIC_ACQ_REL);

    if(prev == 1) {
        vmm_table_free(pagemap_root(vm, 0), 0, PAGEMAP_SIZE / 2, vm->pglevels - 1);
        pagemap_free(vm);
        slab_free(vm);
    }
}

int vmm_map(struct pagemap* restrict vm, uintptr_t virt, uintptr_t phys, unsigned vprot)
{
    bool intr_state = spinlock_acquire_intr(&vm->lock);

    pmentry_type* entry = vmm_pte_walk(vm, virt, true);
    int retval;

    if(entry == nullptr) {
        retval = -ENOMEM;
    }
    else if(pmentry_valid(*entry)) {
        retval = -EEXIST;
    }
    else {
        *entry = pmentry_setup(page_align(phys), vprot);
        retval = 0;
    }

    spinlock_release_intr(&vm->lock, intr_state);

    return retval;
}

int vmm_patch(struct pagemap* restrict vm, uintptr_t virt, unsigned vprot)
{
    bool intr_state = spinlock_acquire_intr(&vm->lock);

    pmentry_type* entry = vmm_pte_walk(vm, virt, false);
    int retval;

    if(entry == nullptr) {
        retval = -ENOMEM;
    }
    else if(!pmentry_valid(*entry)) {
        retval = -EINVAL;
    }
    else {
        *entry = pmentry_setup(pmentry_address(*entry), vprot);
        retval = 0;
    }

    spinlock_release_intr(&vm->lock, intr_state);

    return retval;
}

int vmm_unmap(struct pagemap* restrict vm, uintptr_t virt)
{
    bool intr_state = spinlock_acquire_intr(&vm->lock);

    pmentry_type* entry = vmm_pte_walk(vm, virt, false);
    int retval;

    if(entry == nullptr) {
        retval = -ENOMEM;
    }
    else if(!pmentry_valid(*entry)) {
        retval = -EINVAL;
    }
    else {
        *entry = PMENTRY_NULL;
        retval = 0;
    }

    spinlock_release_intr(&vm->lock, intr_state);

    return retval;
}

uintptr_t vmm_resolve(const struct pagemap* restrict vm, uintptr_t virt)
{
    pmentry_type* entry = vmm_pte_walk((struct pagemap*)vm, virt, false);

    if(entry && pmentry_valid(*entry)) {
        return pmentry_address(*entry) + (virt & (PAGE_SIZE - 1));
    }

    return 0;
}

struct pagemap* vmm_handle_fault(struct pagemap* restrict vm, const struct vmm_fault_info* restrict info)
{
    if(info->present && (info->vprot & VPROT_WRITE)) {
        struct pagemap* clone = vmm_clone(vm);

        if(clone) {
            vmm_switch(clone);
            vmm_release(vm);
            return clone;
        }
    }

    return nullptr;
}

size_t memcpy_sys_to_vm(struct pagemap* restrict vm, uintptr_t v_dst, const void* restrict s_src, size_t sz)
{
    const uint8_t* p_src = s_src;

    while(sz > 0) {
        uintptr_t dst_phys = vmm_resolve(vm, v_dst);

        if(dst_phys == 0) {
            return sz;
        }

        size_t chunk = PAGE_SIZE - (v_dst & (PAGE_SIZE - 1));

        if(chunk > sz) {
            chunk = sz;
        }

        memcpy(phys_to_virt(dst_phys), p_src, chunk);

        v_dst += chunk;
        p_src += chunk;
        sz -= chunk;
    }

    return 0;
}

size_t memcpy_vm_to_sys(void* restrict s_dst, const struct pagemap* restrict vm, uintptr_t v_src, size_t sz)
{
    uint8_t* p_dst = s_dst;

    while(sz) {
        uintptr_t src_phys = vmm_resolve((struct pagemap*)vm, v_src);

        if(src_phys == 0) {
            return sz;
        }

        size_t chunk = PAGE_SIZE - (v_src & (PAGE_SIZE - 1));

        if(chunk > sz) {
            chunk = sz;
        }

        memcpy(p_dst, phys_to_virt(src_phys), chunk);

        v_src += chunk;
        p_dst += chunk;
        sz -= chunk;
    }

    return 0;
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

static void init_vmm(void)
{
    sys_pagemap.lock = SPINLOCK_INIT;
    sys_pagemap.pglevels = vmm_detect_levels();

    int retval = pagemap_alloc(&sys_pagemap);

    if(retval < 0) {
        panic("vmm: out of memory");
    }

    pagemap_zero(&sys_pagemap);

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

initcall_bsp_vmm(init_vmm, INIT_MIDDLE);
