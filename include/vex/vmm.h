/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_VEX_VMM_H
#define INCLUDE_VEX_VMM_H
#include <arch/pml.h>
#include <vex/initcall.h>
#include <vex/vprot.h>

struct pagemap {
    pmentry_t *vm_table;
    uintptr_t vm_table_phys;
};

extern struct pagemap *sys_vm;

struct pagemap *vmm_create(void);
struct pagemap *vmm_fork(struct pagemap *restrict stem);
void vmm_destroy(struct pagemap *restrict vm);
void vmm_switch(struct pagemap *restrict vm);
int vmm_map(struct pagemap *restrict vm, uintptr_t virt, uintptr_t phys, unsigned vprot);
int vmm_patch(struct pagemap *restrict vm, uintptr_t virt, unsigned vprot);
int vmm_unmap(struct pagemap *restrict vm, uintptr_t virt);

INITCALL_EXTERN(vmm);

#endif /* INCLUDE_VEX_VMM_H */
