/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_VMM_H__
#define __INCLUDE_SYS_VMM_H__
#include <machine/pml.h>
#include <sys/initcall.h>
#include <sys/vprot.h>

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

#endif /* __INCLUDE_SYS_VMM_H__ */
