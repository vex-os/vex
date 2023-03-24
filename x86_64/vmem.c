/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/errno.h>
#include <machine/boot.h>
#include <machine/page.h>
#include <machine/vmem.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/iecprefix.h>
#include <sys/kprintf.h>
#include <sys/malloc.h>
#include <sys/pmem.h>

// Kernel's own address space
address_space_t *sys_vm = NULL;

static uint64_t *pml_lookup(uint64_t *restrict pml, size_t at, bool allocate)
{
    uint64_t entry;
    uint64_t *entry_v;

    if(pml[at] & X86_VM_PRESENT) {
        // X86_VM_PRESENT is an x86-specific flag
        // that signals that a table entry exists
        return (uint64_t *)((pml[at] & X86_PTE_ADDR) + hhdm_offset);
    }

    if(allocate) {
        if((entry = pmem_alloc_page()) != 0) {
            pml[at] = (entry | X86_VM_PRESENT);
            entry_v = (uint64_t *)(entry + hhdm_offset);
            memset(entry_v, 0, PAGE_SIZE);
            return entry_v;
        }
    }

    return NULL;
}

static void pml_collapse(uint64_t *restrict pml, size_t a, size_t b, unsigned int level)
{
    size_t i;
    uint64_t *next;

    if(level) {
        for(i = a; i <= b; ++i) {
            if((next = pml_lookup(pml, i, false)) == NULL)
                continue;
            pml_collapse(next, 0, (X86_PML_SIZE - 1), (level - 1));
        }

        pmem_free_page_virt(pml);
    }
}

static uint64_t *pml_lookup_pte(uint64_t *restrict pml, uint64_t virt, bool allocate, unsigned int level)
{

    // Lookup PML4 in PML5
    // UNDONE: implement support for 5-level virtual memory
    if((pml = pml_lookup(pml, ((virt & (UINT64_C(0x1FF) << 48)) >> 48), allocate)) == NULL)
        return NULL;

    // Lookup PML3 in PML4
    if((pml = pml_lookup(pml, ((virt & (UINT64_C(0x1FF) << 39)) >> 39), allocate)) == NULL)
        return NULL;

    // Lookup PML2 in PML3
    if((pml = pml_lookup(pml, ((virt & (UINT64_C(0x1FF) << 30)) >> 30), allocate)) == NULL)
        return NULL;

    // Lookup PML1 in PML2
    if((pml = pml_lookup(pml, ((virt & (UINT64_C(0x1FF) << 21)) >> 21), allocate)) == NULL)
        return NULL;
    
    // Lookup page in PML1
    return &pml[((virt & (UINT64_C(0x1FF) << 12)) >> 12)];
}

address_space_t *vmem_alloc(void)
{
    size_t i;
    uint64_t *vm_src;
    uint64_t *vm_dest;
    address_space_t *vm;

    if((vm = malloc(sizeof(address_space_t))) != NULL) {
        if((vm->vm_table = pmem_alloc_page_virt()) != NULL) {
            memset(vm->vm_table, 0, PAGE_SIZE);

            vm_src = sys_vm->vm_table;
            vm_dest = vm->vm_table;

            for(i = X86_PML_HIGH; i < X86_PML_SIZE; i++) {
                // Copy kernel's memory map
                vm_dest[i] = vm_src[i];
            }

            return vm;
        }

        free(vm);
    }

    return NULL;
}

address_space_t *vmem_fork(address_space_t *restrict vm)
{
    // Implement me
    return NULL;
}

void vmem_free(address_space_t *restrict vm)
{
    // Collapse all table entries except for those that
    // are just references to the kernel's address space
    // UNDONE: implement support for 5-level virtual memory
    pml_collapse(vm->vm_table, 0, (X86_PML_HIGH - 1), (4));

    free(vm);
}

void vmem_switch_to(address_space_t *restrict vm)
{
    // Since we don't set up identity mappings for the kernel, it makes sense
    // to have vm->vm_table to not be a physical pointer but rather a virtual one
    asm volatile("movq %0, %%cr3"::"r"((uintptr_t)vm->vm_table - hhdm_offset):"memory");
}

int vmem_map_page(address_space_t *restrict vm, uintptr_t virt, uintptr_t phys, uint64_t flags)
{
    uint64_t *pte;

    if(flags & (UINT64_C(1) << 32))
    kprintf("vmem_map_page(%p, %p, %p)", (void *)virt, (void *)phys, (void *)flags);

    if((pte = pml_lookup_pte(vm->vm_table, virt, true, (4))) == NULL) {
        // When allocate is true, we can only return NULL
        // if the system is completely out of memory, yikes!
        return ENOMEM;
    }

    if((pte[0] & X86_VM_PRESENT)) {
        // That virtual page is already mapped
        return EINVAL;
    }

    pte[0] = ((phys & X86_PTE_ADDR) | ((flags | X86_VM_PRESENT) & X86_PTE_FLAGS));

    asm volatile("invlpg (%0)" ::"r" (addr) : "memory");

    return 0;
}

int vmem_map_range(address_space_t *restrict vm, uintptr_t start, uintptr_t end, uintptr_t phys, uint64_t flags)
{
    int r;
    uintptr_t virt;

    // Make sure these are aligned
    start = page_align_address(start);
    end = __align_ceil(end, PAGE_SIZE);
    phys = page_align_address(phys);

    kprintf("vmem_map_range(%p, %p, %p, %p)", (void *)start, (void *)end, (void *)phys, (void *)flags);

    virt = start;
    while(virt < end) {
        if((r = vmem_map_page(vm, virt, phys, flags)) != 0)
            return r;
        virt += PAGE_SIZE;
        phys += PAGE_SIZE;
    }

    return 0;
}

int vmem_reflag_page(address_space_t *restrict vm, uintptr_t virt, uint64_t flags)
{
    uint64_t *pte;

    if((pte = pml_lookup_pte(vm->vm_table, virt, false, (4))) == NULL) {
        // Page is not present
        return EINVAL;
    }

    if(!(pte[0] & X86_VM_PRESENT)) {
        // Page is not present... again.
        return EINVAL;
    }

    pte[0] = ((pte[0] & X86_PTE_ADDR) | (flags & X86_PTE_FLAGS));

    return 0;
}

int vmem_unmap_page(address_space_t *restrict vm, uintptr_t virt)
{
    uint64_t *pte;

    if((pte = pml_lookup_pte(vm->vm_table, virt, false, (4))) == NULL) {
        // Page is already not present
        return EINVAL;
    }

    if(!(pte[0] & X86_VM_PRESENT)) {
        // Page is already not present... again.
        return EINVAL;
    }

    pte[0] = UINT64_C(0);

    return 0;
}

int vmem_unmap_range(address_space_t *restrict vm, uintptr_t start, uintptr_t end)
{
    int r;
    uintptr_t virt;

    // Make sure these are aligned
    start = page_align_address(start);
    end = __align_ceil(end, PAGE_SIZE);

    virt = start;
    while(virt < end) {
        if((r = vmem_unmap_page(vm, virt)) != 0)
            return r;
        virt += PAGE_SIZE;
    }

    return 0;
}

uintptr_t vmem_resolve(address_space_t *restrict vm, uintptr_t virt)
{
    uint64_t *pte;

    if((pte = pml_lookup_pte(vm->vm_table, virt, false, (4))) == NULL) {
        // Page is not present
        return 0;
    }

    if(!(pte[0] & X86_VM_PRESENT)) {
        // Page is not present... again.
        return 0;
    }

    return (pte[0] & X86_PTE_ADDR);
}

static void vmem_iterate_memmap(uintptr_t address, size_t n, unsigned short type, void *restrict arg)
{
    int r;
    uintptr_t map_start;
    uintptr_t map_end;
    uintptr_t map_phys;

    kprintf("memmap: type=%04hX, address=%p, n=%zu", type, (void *)address, n);

    if(type != MEMMAP_RESERVED && type != MEMMAP_BAD_MEMORY) {
        map_start = page_align_address(address);
        map_end = __align_ceil((address + n), PAGE_SIZE);
        map_phys = page_align_address(address);

        if(map_end > GiB(4)) {
            if(map_phys < GiB(4)) {
                map_phys = GiB(4);
                map_start = GiB(4) + hhdm_offset;
            }

            // Map range: memmap identity
            if((r = vmem_map_range(sys_vm, map_start, map_end, map_phys, VM_WRITEABLE)) != 0) {
                panic("vmem: unable to map memmap section: %s", strerror(r));
                UNREACHABLE();
            }

            // Map range: memmap
            map_start += hhdm_offset;
            map_end += hhdm_offset;
            if((r = vmem_map_range(sys_vm, map_start, map_end, map_phys, VM_WRITEABLE | VM_NOEXECUTE)) != 0) {
                panic("vmem: unable to map memmap section: %s", strerror(r));
                UNREACHABLE();
            }
        }
    }
}

static void init_vmem(void)
{
    int r;
    size_t i;
    uintptr_t map_start;
    uintptr_t map_end;
    uintptr_t map_phys;

    if((sys_vm = malloc(sizeof(address_space_t))) == NULL) {
        panic("vmem: insufficient memory");
        UNREACHABLE();
    }

    if((sys_vm->vm_table = pmem_alloc_page_virt()) == NULL) {
        panic("vmem: insufficient memory");
        UNREACHABLE();
    }

    memset(sys_vm->vm_table, 0, PAGE_SIZE);

    for(i = X86_PML_HIGH; i < X86_PML_SIZE; i++) {
        if(!pml_lookup(sys_vm->vm_table, i, true)) {
            panic("vmem: insufficient memory");
            UNREACHABLE();
        }
    }

    // Map range: kernel binary .text section
    map_start = page_align_address((uintptr_t)sys_text_start);
    map_end = __align_ceil((uintptr_t)sys_text_end, PAGE_SIZE);
    map_phys = (map_start - kernel_base_virt + kernel_base_phys);
    if((r = vmem_map_range(sys_vm, map_start, map_end, map_phys, 0 | (UINT64_C(1) << 32))) != 0) {
        panic("vmem: unable to map kernel text section: %s", strerror(r));
        UNREACHABLE();
    }

    // Map range: kernel binary .rodata section
    map_start = page_align_address((uintptr_t)sys_rodata_start);
    map_end = __align_ceil((uintptr_t)sys_rodata_end, PAGE_SIZE);
    map_phys = (map_start - kernel_base_virt + kernel_base_phys);
    if((r = vmem_map_range(sys_vm, map_start, map_end, map_phys, VM_NOEXECUTE | (UINT64_C(1) << 32))) != 0) {
        panic("vmem: unable to map kernel rodata section: %s", strerror(r));
        UNREACHABLE();
    }

    // Map range: kernel binary .data section
    map_start = page_align_address((uintptr_t)sys_data_start);
    map_end = __align_ceil((uintptr_t)sys_data_end, PAGE_SIZE);
    map_phys = (map_start - kernel_base_virt + kernel_base_phys);
    if((r = vmem_map_range(sys_vm, map_start, map_end, map_phys, VM_WRITEABLE | VM_NOEXECUTE | (UINT64_C(1) << 32))) != 0) {
        panic("vmem: unable to map kernel data section: %s", strerror(r));
        UNREACHABLE();
    }

    // Map range: kernel binary .bss section
    map_start = page_align_address((uintptr_t)sys_bss_start);
    map_end = __align_ceil((uintptr_t)sys_bss_end, PAGE_SIZE);
    map_phys = (map_start - kernel_base_virt + kernel_base_phys);
    if((r = vmem_map_range(sys_vm, map_start, map_end, map_phys, VM_WRITEABLE | VM_NOEXECUTE | (UINT64_C(1) << 32))) != 0) {
        panic("vmem: unable to map kernel bss section: %s", strerror(r));
        UNREACHABLE();
    }

    // Map range: IO space (first 4 GiB)
    map_start = hhdm_offset + 0x1000;
    map_end = hhdm_offset + GiB(4);
    map_phys = 0x1000;
    if((r = vmem_map_range(sys_vm, map_start, map_end, map_phys, VM_WRITEABLE | VM_NOEXECUTE)) != 0) {
        panic("vmem: unable to map IO space: %s", strerror(r));
        UNREACHABLE();
    }

    // Map range: IO space (first 4 GiB) identity map
    map_start = 0x1000;
    map_end = GiB(4);
    map_phys = 0x1000;
    if((r = vmem_map_range(sys_vm, map_start, map_end, map_phys, VM_WRITEABLE)) != 0) {
        panic("vmem: unable to identity map IO space: %s", strerror(r));
        UNREACHABLE();
    }

    // Map ranges: bootloader memmap (after the IO space)
    iterate_memmap(&vmem_iterate_memmap, NULL);

    kprintf("vmem: resolve %p : %p", (void *)0xffffffff80005000, (void *)vmem_resolve(sys_vm, 0xffffffff80005000));
    kprintf("vmem: switching to sys_vm (vm_table at %p)", sys_vm->vm_table);
    vmem_switch_to(sys_vm);
    kprintf("vmem: hopefully switched to sys_vm");
}
early_initcall(vmem, init_vmem);
initcall_dependency(vmem, boot);
initcall_dependency(vmem, pmem);
initcall_dependency(vmem, malloc);
