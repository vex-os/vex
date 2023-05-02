/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/errno.h>
#include <machine/boot.h>
#include <machine/page.h>
#include <machine/vmem.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/iecprefix.h>
#include <sys/kprintf.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/pmem.h>

address_space_t *sys_space = NULL;

static uint64_t *pml_lookup(uint64_t *restrict table, size_t index, bool allocate)
{
    uint64_t *entry;

    if((table[index] & X86_VM_PRESENT)) {
        // Entries that have X86_VM_PRESENT bit
        // set are assumed to contain a valid address
        return (uint64_t *)((table[index] & X86_PTE_ADDR) + hhdm_offset);
    }

    if(allocate) {
        if((entry = pmem_alloc_virt_page()) != NULL) {
            table[index] = ((uintptr_t)(entry) - hhdm_offset) | X86_VM_PRESENT;
            memset(entry, 0, PAGE_SIZE);
            return entry;
        }
    }

    return NULL;
}

static void pml_collapse(uint64_t *restrict table, size_t begin, size_t end, unsigned int level)
{
    size_t i;
    uint64_t *next;

    if(level != 0) {
        for(i = begin; i < end; i++) {
            if((next = pml_lookup(table, i, false)) == NULL)
                continue;
            pml_collapse(next, 0, X86_PML_SIZE, (level - 1));
        }

        pmem_free_virt_page(table);
    }
}

static uint64_t *pml_lookup_pte(uint64_t *restrict table, uint64_t virt, bool allocate)
{
    if((table = pml_lookup(table, ((virt & (UINT64_C(0x1FF) << 39)) >> 39), allocate)) == NULL)
        return NULL;
    if((table = pml_lookup(table, ((virt & (UINT64_C(0x1FF) << 30)) >> 30), allocate)) == NULL)
        return NULL;
    if((table = pml_lookup(table, ((virt & (UINT64_C(0x1FF) << 21)) >> 21), allocate)) == NULL)
        return NULL;
    return &table[((virt & (UINT64_C(0x1FF) << 12)) >> 12)];
}

address_space_t *vmem_alloc(void)
{
    size_t i;
    uint64_t *src;
    uint64_t *dest;
    address_space_t *space;

    if((space = malloc(sizeof(address_space_t))) != NULL) {
        if((space->vm_table = pmem_alloc_virt_page()) != NULL) {
            memset(space->vm_table, 0, PAGE_SIZE);

            src = sys_space->vm_table;
            dest = space->vm_table;

            for(i = X86_PML_HIGH; i < X86_PML_SIZE; ++i) {
                // Copy over kernel's higher half mappings
                // to the newly created address space
                dest[i] = src[i];
            }

            return space;
        }

        free(space);
    }

    return NULL;
}

address_space_t *vmem_copy(address_space_t *restrict space)
{
    return NULL;
}

void vmem_free(address_space_t *restrict space)
{
    pml_collapse(space->vm_table, 0, X86_PML_HIGH, 4);
    free(space);
}

void vmem_switch_to(address_space_t *restrict space)
{
    asm volatile("movq %0, %%cr3"::"r"((uintptr_t)(space->vm_table) - hhdm_offset):"memory");
}

int vmem_map(address_space_t *restrict space, uintptr_t virt, uintptr_t phys, uintmax_t flags)
{
    uint64_t *entry;

    virt = page_align_address(virt);
    phys = page_align_address(phys);

    if((entry = pml_lookup_pte(space->vm_table, virt, true)) == NULL) {
        // Out of memory. Too bad!
        return ENOMEM;
    }

    if((entry[0] & X86_VM_PRESENT)) {
        // Page already mapped. Too bad!
        return EINVAL;
    }

    entry[0] = ((phys & X86_PTE_ADDR) | ((flags | X86_VM_PRESENT) & X86_PTE_FLAGS));

    asm volatile("invlpg (%0)"::"r"(virt):"memory");

    return 0;
}

int vmem_unmap(address_space_t *restrict space, uintptr_t virt)
{
    uint64_t *entry;

    virt = page_align_address(virt);

    if((entry = pml_lookup_pte(space->vm_table, virt, true)) == NULL) {
        // Out of memory. Too bad!
        return ENOMEM;
    }

    if(!(entry[0] & X86_VM_PRESENT)) {
        // Page not present
        return EINVAL;
    }

    entry[0] = UINT64_C(0);

    return 0;
}

static void vmem_map_section(const void *restrict start, const void *restrict end, const char *restrict name, uintmax_t flags)
{
    uintptr_t virt;
    uintptr_t phys;
    uintptr_t vstart = page_align_address((uintptr_t)(start));
    uintptr_t vend = __align_ceil((uintptr_t)(end), PAGE_SIZE);

    kprintf("vmem: mapping section %s", name);

    for(virt = vstart; virt < vend; virt += PAGE_SIZE) {
        phys = virt - kernel_base_virt + kernel_base_phys;
        kassert(vmem_map(sys_space, virt, phys, flags) == 0);
    }
}

static void vmem_iterate_memmap(uintptr_t address, size_t n, unsigned short type, void *restrict arg)
{
    uintptr_t addr;
    uintptr_t start;
    uintptr_t end;

    kprintf("memmap: type=%04hX, address=%p, n=%zu", type, (void *)address, n);

    if(type != MEMMAP_RESERVED && type != MEMMAP_BAD_MEMORY) {
        start = page_align_address(address);
        end = __align_ceil((address + n), PAGE_SIZE);

        if(end > GiB(4)) {
            for(addr = start; addr < end; addr += PAGE_SIZE) {
                if(addr > GiB(4)) {
                    kassert(vmem_map(sys_space, addr, addr, VM_WRITEABLE));
                    kassert(vmem_map(sys_space, addr + hhdm_offset, addr, VM_WRITEABLE | VM_NOEXECUTE));
                }
            }
        }
    }
}

static void init_vmem(void)
{
    size_t i;
    uintptr_t phys;

    if((sys_space = malloc(sizeof(address_space_t))) == NULL) {
        panic("vmem: insufficient memory");
        UNREACHABLE();
    }

    if((sys_space->vm_table = pmem_alloc_virt_page()) == NULL) {
        panic("vmem: insufficient memory");
        UNREACHABLE();
    }

    memset(sys_space->vm_table, 0, PAGE_SIZE);

    for(i = X86_PML_HIGH; i < X86_PML_SIZE; ++i) {
        if(!pml_lookup(sys_space->vm_table, i, true)) {
            panic("vmem: insufficient memory");
            UNREACHABLE();
        }
    }

    vmem_map_section(sys_text_start, sys_text_end, "text", 0);
    vmem_map_section(sys_rodata_start, sys_rodata_end, "rodata", VM_NOEXECUTE);
    vmem_map_section(sys_data_start, sys_data_end, "data", VM_WRITEABLE | VM_NOEXECUTE);
    vmem_map_section(sys_bss_start, sys_bss_end, "bss", VM_WRITEABLE | VM_NOEXECUTE);

    for(phys = PAGE_SIZE; phys < GiB(4); phys += PAGE_SIZE) {
        kassert(vmem_map(sys_space, phys, phys, VM_WRITEABLE) == 0);
        kassert(vmem_map(sys_space, phys + hhdm_offset, phys, VM_WRITEABLE | VM_NOEXECUTE) == 0);
    }

    iterate_memmap(&vmem_iterate_memmap, NULL);

    kprintf("vmem: hhdm_offset=%p", (void *)hhdm_offset);
    kprintf("vmem: switching to sys_space (vm_table at %p)", sys_space->vm_table);
    vmem_switch_to(sys_space);
    kprintf("vmem: hopefully switched to sys_space");
}
early_initcall(vmem, init_vmem);
initcall_dependency(vmem, boot);
initcall_dependency(vmem, malloc);
initcall_dependency(vmem, pmem);
