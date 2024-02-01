/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#ifndef INCLUDE_KERN_VMM_H
#define INCLUDE_KERN_VMM_H
#include <machine/pml.h>
#include <kern/initcall.h>
#include <kern/vprot.h>

struct pagemap {
    uintptr_t vm_pml_phys;
    vmm_pml_t *vm_pml;
};

extern struct pagemap *sys_vm;

struct pagemap *vmm_create(void);
struct pagemap *vmm_fork(struct pagemap *restrict stem);
void vmm_destroy(struct pagemap *restrict vm);
void vmm_switch(struct pagemap *restrict vm);
int vmm_map(struct pagemap *restrict vm, uintptr_t virt, uintptr_t phys, unsigned short vprot);
int vmm_patch(struct pagemap *restrict vm, uintptr_t virt, unsigned short vprot);
int vmm_unmap(struct pagemap *restrict vm, uintptr_t virt);

initcall_extern(vmm);

#endif /* INCLUDE_KERN_VMM_H */
