#include <vex/errno.h>
#include <vex/hhdm.h>
#include <vex/kbase.h>
#include <vex/kprintf.h>
#include <vex/limine.h>
#include <vex/linker.h>
#include <vex/memmap.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pmm.h>
#include <vex/slab.h>
#include <vex/string.h>
#include <vex/vmm.h>

#if __x86_64__
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

static volatile __limine_request struct limine_paging_mode_request paging_mode = {
    .id = LIMINE_PAGING_MODE_REQUEST,
    .min_mode = PAGING_LEVEL_3,
    .max_mode = PAGING_LEVEL_5,
    .revision = 2,
};

static bool pagemap_lvl3 = false;
static bool pagemap_lvl4 = false;
static bool pagemap_lvl5 = false;

struct pagemap sys_pagemap;

static size_t pmentry_index(uintptr_t virt, uintptr_t mask, uintptr_t shift)
{
    // This assumes the target architecture uses
    // x86-like pmentry_t format where a part of the virtual
    // address is an index of a specific level page table.
    // This should work without a fuss on both ARM64 and RISC-V.
    return (size_t)((virt & (mask << shift)) >> shift);
}

static pmentry_t* get_pmentry(pmentry_t* restrict table, size_t index, bool allocate)
{
    pmentry_t* entry;
    uintptr_t address;

    if(!pmentry_valid(table[index])) {
        if(allocate) {
            address = pmm_alloc();

            if(address) {
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

static pmentry_t* lookup_pmentry(pmentry_t* restrict table, uintptr_t virt, bool allocate)
{
    size_t index;

    if(pagemap_lvl5) {
        index = pmentry_index(virt, PMENTRY_LVL5_MASK, PMENTRY_LVL5_SHIFT);
        table = get_pmentry(table, index, allocate);

        if(table == NULL) {
            return NULL;
        }
    }

    if(pagemap_lvl4) {
        index = pmentry_index(virt, PMENTRY_LVL4_MASK, PMENTRY_LVL4_SHIFT);
        table = get_pmentry(table, index, allocate);

        if(table == NULL) {
            return NULL;
        }
    }

    if(pagemap_lvl3) {
        index = pmentry_index(virt, PMENTRY_LVL3_MASK, PMENTRY_LVL3_SHIFT);
        table = get_pmentry(table, index, allocate);

        if(table == NULL) {
            return NULL;
        }
    }

    index = pmentry_index(virt, PMENTRY_LVL2_MASK, PMENTRY_LVL2_SHIFT);
    table = get_pmentry(table, index, allocate);

    if(table == NULL) {
        return NULL;
    }

    index = pmentry_index(virt, PMENTRY_LVL1_MASK, PMENTRY_LVL1_SHIFT);
    return &table[index];
}

static void pmentry_collapse(pmentry_t* restrict table, size_t begin, size_t end, unsigned int level)
{
    size_t i;
    pmentry_t* next;

    if(level) {
        for(i = begin; i < end; ++i) {
            next = lookup_pmentry(table, i, false);

            if(next) {
                pmentry_collapse(next, 0U, PAGEMAP_SIZE, level - 1U);
            }
        }

        pmm_free_hhdm(table);
    }
}

struct pagemap* vmm_create(void)
{
    size_t i;
    struct pagemap* vm = slab_alloc(sizeof(struct pagemap));

    if(vm) {
        vm->virt = pmm_alloc_hhdm();
        vm->phys = hhdm_to_phys(vm->virt);

        if(vm->virt) {
            memset(vm->virt, 0, PAGE_SIZE);

            for(i = PAGEMAP_KERN; i < PAGEMAP_SIZE; ++i) {
                // PATCHME: implement a trampoline so the userspace
                // cannot see the kernel memory mappings directly
                vm->virt[i] = sys_pagemap.virt[i];
            }

            return vm;
        }
    }

    return NULL;
}

struct pagemap* vmm_fork(struct pagemap* restrict stem)
{
    panic("vmm_fork is not implemented");
    unreachable();
}

void vmm_destroy(struct pagemap* restrict vm)
{
    if(pagemap_lvl5) {
        pmentry_collapse(vm->virt, 0U, PAGEMAP_KERN, 5U);
        goto cleanup;
    }

    if(pagemap_lvl4) {
        pmentry_collapse(vm->virt, 0U, PAGEMAP_KERN, 4U);
        goto cleanup;
    }

    if(pagemap_lvl3) {
        pmentry_collapse(vm->virt, 0U, PAGEMAP_KERN, 3U);
        goto cleanup;
    }

    pmentry_collapse(vm->virt, 0U, PAGEMAP_KERN, 2U);

cleanup:
    pmm_free_hhdm(vm->virt);
    slab_free(vm);
}

void vmm_switch(struct pagemap* restrict vm)
{
    pagemap_switch(vm->phys);
}

int vmm_map(struct pagemap* restrict vm, uintptr_t virt, uintptr_t phys, unsigned int vprot)
{
    pmentry_t* entry = lookup_pmentry(vm->virt, virt, true);

    if(entry) {
        if(!pmentry_valid(entry[0])) {
            entry[0] = make_pmentry(page_align(phys), vprot);
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

int vmm_patch(struct pagemap* restrict vm, uintptr_t virt, unsigned int vprot)
{
    pmentry_t* entry = lookup_pmentry(vm->virt, virt, false);

    if(entry) {
        if(pmentry_valid(entry[0])) {
            entry[0] = make_pmentry(pmentry_address(entry[0]), vprot);
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

int vmm_unmap(struct pagemap* restrict vm, uintptr_t virt)
{
    pmentry_t* entry = lookup_pmentry(vm->virt, virt, false);

    if(entry) {
        if(pmentry_valid(entry[0])) {
            entry[0] = PMENTRY_NULL;
            return 0;
        }

        return EINVAL;
    }

    return ENOMEM;
}

uintptr_t vmm_resolve(struct pagemap* restrict vm, uintptr_t virt)
{
    pmentry_t* entry = lookup_pmentry(vm->virt, virt, false);

    if(entry && pmentry_valid(entry[0])) {
        return pmentry_address(entry[0]) + (virt & (PAGE_SIZE - 1U));
    }

    return 0U;
}

static int vmm_map_sys_section(const void* restrict start, const void* restrict end, unsigned int vprot)
{
    int retval;
    uintptr_t phys;
    uintptr_t virt;
    uintptr_t virt_end;

    virt = page_align((uintptr_t)(start));
    virt_end = page_align_up((uintptr_t)(end));
    phys = virt - kbase_virt + kbase_phys;

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
    uintptr_t phys;
    uintptr_t virt;
    uintptr_t virt_end;

    phys = page_align(entry->base);
    virt = page_align(entry->base) + hhdm_base;
    virt_end = page_align_up(virt + entry->length);

    while(virt < virt_end) {
        retval = vmm_map(&sys_pagemap, virt, phys, VPROT_RWX);

        if(retval == 0 || retval == EINVAL) {
            virt += PAGE_SIZE;
            phys += PAGE_SIZE;
            continue;
        }

        return retval;
    }

    return 0;

    return EINVAL;
}

void init_vmm(void)
{
    size_t i;
    int retval;

    pagemap_lvl3 = false;
    pagemap_lvl4 = false;
    pagemap_lvl5 = false;

    if(paging_mode.response->mode >= PAGING_LEVEL_3)
        pagemap_lvl3 = true;
    if(paging_mode.response->mode >= PAGING_LEVEL_4)
        pagemap_lvl4 = true;
    if(paging_mode.response->mode >= PAGING_LEVEL_5)
        pagemap_lvl5 = true;

    sys_pagemap.virt = pmm_alloc_hhdm();
    sys_pagemap.phys = hhdm_to_phys(sys_pagemap.virt);

    if(sys_pagemap.virt == NULL) {
        panic("vmm: out of memory: sys_pagemap.virt");
        unreachable();
    }

    memset(sys_pagemap.virt, 0, PAGE_SIZE);

    for(i = PAGEMAP_KERN; i < PAGEMAP_SIZE; ++i) {
        if(NULL == get_pmentry(sys_pagemap.virt, i, true)) {
            panic("vmm: out of memory: sys_pagemap.virt[%zu]", i);
            unreachable();
        }
    }

    retval = vmm_map_sys_section(&text_start, &text_end, VPROT_RX);

    if(retval) {
        panic("vmm: failed to map kernel section [text]: %s", strerror(retval));
        unreachable();
    }

    retval = vmm_map_sys_section(&rodata_start, &rodata_end, VPROT_READ);

    if(retval) {
        panic("vmm: failed to map kernel section [rodata]: %s", strerror(retval));
        unreachable();
    }

    retval = vmm_map_sys_section(&data_start, &data_end, VPROT_RW);

    if(retval) {
        panic("vmm: failed to map kernel section [data]: %s", strerror(retval));
        unreachable();
    }

    for(i = 0U; i < memmap->entry_count; ++i) {
        retval = vmm_map_sys_memmap(memmap->entries[i]);

        if(retval && retval != EINVAL) {
            panic("vmm: failed to map memory map entry [%zu]: %s", i, strerror(retval));
            unreachable();
        }
    }

    vmm_switch(&sys_pagemap);

    kprintf(KP_INFORM, "vmm: sys_pagemap=%p", (const void*)(sys_pagemap.phys));
}
