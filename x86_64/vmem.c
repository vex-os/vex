/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <stdbool.h>
#include <sys/boot.h>
#include <sys/kprintf.h>
#include <sys/pmalloc.h>
#include <sys/slab.h>
#include <sys/vmem.h>

// Kernel address space
vmem_space_t *sys_space = NULL;

vmem_space_t *vmem_create(void)
{
    // Implement me
    return NULL;
}

vmem_space_t *vmem_fork(vmem_space_t *restrict parent)
{
    // Implement me
    return NULL;
}

void vmem_delete(vmem_space_t *restrict space)
{
    // Implement me
}

void vmem_switch(vmem_space_t *restrict space)
{
    // Implement me
}

int vmem_map_page(vmem_space_t *restrict space, uintptr_t virt, uintptr_t phys, uintmax_t flags)
{
    // Implement me
    return 0;
}

int vmem_unmap_page(vmem_space_t *restrict space, uintptr_t virt)
{
    // Implement me
    return 0;
}

uintptr_t vmem_resolve(vmem_space_t *restrict space, uintptr_t virt)
{
    // Implement me
    return 0;
}

static void init_vmem(void)
{
    // Implement me
}
early_initcall(vmem, init_vmem);
initcall_dependency(vmem, slab);
