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

#if __amd64__
#define PAGING_LEVEL_3 LIMINE_PAGING_MODE_X86_64_4LVL
#define PAGING_LEVEL_4 LIMINE_PAGING_MODE_X86_64_4LVL
#define PAGING_LEVEL_5 LIMINE_PAGING_MODE_X86_64_5LVL
#elif __aarch64__
#define PAGING_LEVEL_3 LIMINE_PAGING_MODE_AARCH64_4LVL
#define PAGING_LEVEL_4 LIMINE_PAGING_MODE_AARCH64_4LVL
#define PAGING_LEVEL_5 LIMINE_PAGING_MODE_AARCH64_5LVL
#elif __riscv64__
#define PAGING_LEVEL_3 LIMINE_PAGING_MODE_RISCV_SV39
#define PAGING_LEVEL_4 LIMINE_PAGING_MODE_RISCV_SV48
#define PAGING_LEVEL_5 LIMINE_PAGING_MODE_RISCV_SV57
#else
#error Unknown architecture
#endif

static volatile __used __section(".limine_requests") struct limine_paging_mode_request paging_mode_request = {
    .id = LIMINE_PAGING_MODE_REQUEST_ID,
    .revision = 2,
    .min_mode = PAGING_LEVEL_3,
    .max_mode = PAGING_LEVEL_5,
};

struct pagemap sys_pagemap;

static __nodiscard pmentry_t* alloc_table(void)
{
    pmentry_t* result = pslab_alloc_virt();

    if(result) {
        memset(result, 0, PAGE_SIZE);
    }

    return result;
}

static pmentry_t* lookup_entry(pmentry_t* restrict table, uintptr_t virt, int levels, int stop, bool allocate)
{
    int level;
    size_t index;
    pmentry_t entry;
    pmentry_t* child;

    for(level = levels; level > stop; level -= 1) {
        index = pmentry_index(virt, level);
        entry = table[index];

        if(pmentry_is_valid(entry)) {
            if(pmentry_is_leaf(entry))
                return &table[index];
            table = phys_to_virt(pmentry_address(entry));
        }
        else if(allocate) {
            child = alloc_table();

            if(child == NULL) {
                return NULL;
            }

            table[index] = make_pmentry(virt_to_phys(child), VPROT_URWX);
            table = child;
        }
        else {
            return NULL;
        }
    }

    return &table[pmentry_index(virt, stop)];
}

static void free_subtables(pmentry_t* restrict table, int level)
{
    size_t i;
    pmentry_t entry;
    pmentry_t* child;

    if(level > 1) {
        for(i = 0; i < PAGEMAP_KERN; ++i) {
            entry = table[i];

            if(pmentry_is_valid(entry) && !pmentry_is_leaf(entry)) {
                child = phys_to_virt(pmentry_address(entry));
                free_subtables(child, level - 1);
                pslab_free_virt(child);
            }
        }
    }
}

static int clone_subtables(pmentry_t* restrict dest, const pmentry_t* restrict src, int level)
{
    size_t i;
    pmentry_t entry;
    pmentry_t* child_src;
    pmentry_t* child_dest;
    int retval;

    for(i = 0; i < PAGEMAP_KERN; ++i) {
        entry = src[i];

        if(!pmentry_is_valid(entry)) {
            continue;
        }

        if(level <= 1 || pmentry_is_leaf(entry)) {
            dest[i] = entry;
        }
        else {
            child_src = phys_to_virt(pmentry_address(entry));
            child_dest = alloc_table();

            if(child_dest == NULL) {
                return ENOMEM;
            }

            dest[i] = make_pmentry(virt_to_phys(child_dest), VPROT_URWX);
            retval = clone_subtables(child_dest, child_src, level - 1);

            if(retval) {
                return retval;
            }
        }
    }

    return 0;
}

static void cow_subtables(pmentry_t* restrict vma, pmentry_t* restrict vmb, int level)
{
    size_t i;
    pmentry_t entry;
    pmentry_t readonly;
    pmentry_t* c_vma;
    pmentry_t* c_vmb;
    unsigned int vprot;

    for(i = 0; i < PAGEMAP_KERN; ++i) {
        entry = vma[i];

        if(!pmentry_is_valid(entry)) {
            continue;
        }

        if(level <= 1 || pmentry_is_leaf(entry)) {
            vprot = VPROT_READ | VPROT_USER;

            if(pmentry_is_leaf(entry)) {
                switch(level) {
                    case 2:
                        vprot |= VPROT_BIG1;
                        break;

                    case 3:
                        vprot |= VPROT_BIG2;
                        break;
                }
            }

            readonly = make_pmentry(pmentry_address(entry), vprot);
            vma[i] = readonly;
            vmb[i] = readonly;
        }
        else {
            c_vma = phys_to_virt(pmentry_address(vma[i]));
            c_vmb = phys_to_virt(pmentry_address(vmb[i]));
            cow_subtables(c_vma, c_vmb, level - 1);
        }
    }
}

struct pagemap* vmm_create(void)
{
    size_t i;
    pmentry_t* root;
    struct pagemap* vm;

    root = alloc_table();

    if(root == NULL) {
        return NULL;
    }

    vm = slab_alloc(sizeof(struct pagemap));

    if(vm == NULL) {
        pslab_free_virt(root);
        return NULL;
    }

    // Share pre-allocated kernel higher-half entries
    for(i = PAGEMAP_KERN; i < PAGEMAP_SIZE; ++i) {
        root[i] = sys_pagemap.virt[i];
    }

    vm->virt = root;
    vm->phys = virt_to_phys(root);
    vm->levels = sys_pagemap.levels;

    return vm;
}

struct pagemap* vmm_fork(const struct pagemap* restrict stem)
{
    struct pagemap* vm = vmm_clone(stem);

    if(vm == NULL) {
        return NULL;
    }

    cow_subtables(stem->virt, vm->virt, vm->levels);

    return vm;
}

struct pagemap* vmm_clone(const struct pagemap* restrict stem)
{
    size_t i;
    int retval;
    pmentry_t* root;
    struct pagemap* vm;

    root = alloc_table();

    if(root == NULL) {
        return NULL;
    }

    vm = slab_alloc(sizeof(struct pagemap));

    if(vm == NULL) {
        pslab_free_virt(root);
        return NULL;
    }

    retval = clone_subtables(root, stem->virt, stem->levels);

    if(retval) {
        free_subtables(root, stem->levels);
        pslab_free_virt(root);
        slab_free(vm);
        return NULL;
    }

    for(i = PAGEMAP_KERN; i < PAGEMAP_SIZE; ++i) {
        root[i] = sys_pagemap.virt[i];
    }

    vm->virt = root;
    vm->phys = virt_to_phys(root);
    vm->levels = stem->levels;

    return vm;
}

void vmm_destroy(struct pagemap* restrict vm)
{
    free_subtables(vm->virt, vm->levels);
    pslab_free_virt(vm->virt);
    slab_free(vm);
}

void vmm_switch(const struct pagemap* restrict vm)
{
    pagemap_switch(vm->phys);
}

int vmm_map(struct pagemap* restrict vm, uintptr_t virt, uintptr_t phys, unsigned int vprot)
{
    int stop = pmentry_level(vprot);
    pmentry_t* entry = lookup_entry(vm->virt, virt, vm->levels, stop, true);

    if(entry == NULL) {
        return ENOMEM;
    }

    if(pmentry_is_valid(*entry)) {
        return EEXIST;
    }

    *entry = make_pmentry(page_align(phys), vprot);

    return 0;
}

int vmm_patch(struct pagemap* restrict vm, uintptr_t virt, unsigned int vprot)
{
    pmentry_t* entry = lookup_entry(vm->virt, virt, vm->levels, 1, false);

    if(entry == NULL || !pmentry_is_valid(*entry)) {
        return ENOENT;
    }

    *entry = make_pmentry(pmentry_address(*entry), vprot);
    pagemap_flush_virt(virt);
    return 0;
}

int vmm_unmap(struct pagemap* restrict vm, uintptr_t virt)
{
    pmentry_t* entry = lookup_entry(vm->virt, virt, vm->levels, 1, false);

    if(entry == NULL || !pmentry_is_valid(*entry)) {
        return ENOENT;
    }

    *entry = PMENTRY_NULL;
    pagemap_flush_virt(virt);
    return 0;
}

uintptr_t vmm_resolve(const struct pagemap* restrict vm, uintptr_t virt)
{
    int level;
    pmentry_t entry;
    pmentry_t* table = vm->virt;
    uintptr_t mask;

    for(level = vm->levels; level >= 1; level -= 1) {
        entry = table[pmentry_index(virt, level)];

        if(!pmentry_is_valid(entry)) {
            return 0;
        }

        if(level == 1 || pmentry_is_leaf(entry)) {
            mask = ((uintptr_t)(PAGE_SIZE) << (9U * (unsigned)(level - 1))) - 1U;
            return pmentry_address(entry) | (virt & mask);
        }

        table = phys_to_virt(pmentry_address(entry));
    }

    return 0;
}

static int vmm_map_range(uintptr_t virt, uintptr_t phys, uintptr_t length, unsigned int base_vprot)
{
    int retval;
    uintptr_t end;
    uintptr_t remain;

    end = virt + length;

    while(virt < end) {
        remain = end - virt;

        if(remain >= PAGE_SIZE_BIG2 && (virt & (PAGE_SIZE_BIG2 - 1)) == 0 && (phys & (PAGE_SIZE_BIG2 - 1)) == 0) {
            retval = vmm_map(&sys_pagemap, virt, phys, base_vprot | VPROT_BIG2);
            if(retval && retval != EEXIST)
                return retval;
            virt += PAGE_SIZE_BIG2;
            phys += PAGE_SIZE_BIG2;
            continue;
        }

        if(remain >= PAGE_SIZE_BIG1 && (virt & (PAGE_SIZE_BIG1 - 1)) == 0 && (phys & (PAGE_SIZE_BIG1 - 1)) == 0) {
            retval = vmm_map(&sys_pagemap, virt, phys, base_vprot | VPROT_BIG1);
            if(retval && retval != EEXIST)
                return retval;
            virt += PAGE_SIZE_BIG1;
            phys += PAGE_SIZE_BIG1;
            continue;
        }

        retval = vmm_map(&sys_pagemap, virt, phys, base_vprot);
        if(retval && retval != EEXIST)
            return retval;
        virt += PAGE_SIZE;
        phys += PAGE_SIZE;
    }

    return 0;
}

static int vmm_map_section(const char* restrict start, const char* restrict end, unsigned int vprot)
{
    uintptr_t virt = page_align((uintptr_t)start);
    uintptr_t virt_end = page_align_up((uintptr_t)end);
    uintptr_t phys = virt - kbase_virt + kbase_phys;
    return vmm_map_range(virt, phys, virt_end - virt, vprot);
}

static int vmm_map_memmap_entry(const struct limine_memmap_entry* restrict entry)
{
    uintptr_t phys = page_align(entry->base);
    uintptr_t virt = phys + hhdm_base;
    uintptr_t length = page_align_up(entry->base + entry->length) - phys;
    return vmm_map_range(virt, phys, length, VPROT_KRW);
}

static int init_vmm(void)
{
    size_t i;
    int levels;
    int retval;
    uint64_t mode;
    pmentry_t* root;
    pmentry_t* child;

    if(paging_mode_request.response == NULL) {
        panic("bootloader did not negotiate paging mode");
    }

    mode = paging_mode_request.response->mode;

    if(mode >= PAGING_LEVEL_5) {
        levels = 5;
    }
    else if(mode >= PAGING_LEVEL_4) {
        levels = 4;
    }
    else if(mode >= PAGING_LEVEL_3) {
        levels = 3;
    }
    else {
        panic("unsupported paging mode %zu", (size_t)(mode));
    }

    root = alloc_table();

    if(root == NULL) {
        panic("vmm: out of memory allocating sys_pagemap root");
    }

    sys_pagemap.virt = root;
    sys_pagemap.phys = virt_to_phys(root);
    sys_pagemap.levels = levels;

    for(i = PAGEMAP_KERN; i < PAGEMAP_SIZE; ++i) {
        child = alloc_table();

        if(child == NULL) {
            panic("vmm: out of memory preallocating kernel entries");
        }

        root[i] = make_pmentry(virt_to_phys(child), VPROT_KRW);
    }

    retval = vmm_map_section(text_start, text_end, VPROT_KRX);

    if(retval) {
        panic("vmm: failed to map .text: %d", retval);
    }

    retval = vmm_map_section(rodata_start, rodata_end, VPROT_READ);

    if(retval) {
        panic("vmm: failed to map .rodata: %d", retval);
    }

    retval = vmm_map_section(data_start, data_end, VPROT_KRW);

    if(retval) {
        panic("vmm: failed to map .data: %d", retval);
    }

    for(i = 0; i < memmap->entry_count; ++i) {
        retval = vmm_map_memmap_entry(memmap->entries[i]);

        if(retval) {
            panic("vmm: failed to map memmap entry %zu: %d", i, retval);
        }
    }

    vmm_switch(&sys_pagemap);

    kprintf(KP_INFORM, "vmm: active, sys_pagemap phys=%p levels=%d", (const void*)sys_pagemap.phys, sys_pagemap.levels);

    return 0;
}

DEFINE_INITFN(init_vmm, INITFN_CAT_VMM, INITFN_ORDER_MIDDLE);
