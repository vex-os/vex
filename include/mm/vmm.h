/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_MM_VMM_H
#define INCLUDE_MM_VMM_H
#include <arch/paging.h>
#include <core/compiler.h>
#include <mm/vprot.h>

struct pagemap {
    pmentry_t *vm_virt;
    uintptr_t vm_phys;
};

extern struct pagemap *sys_vm;

struct pagemap *vmm_create(void);
struct pagemap *vmm_fork(struct pagemap *restrict stem);
void vmm_destroy(struct pagemap *restrict vm);
void vmm_switch(struct pagemap *restrict vm);
int vmm_map(struct pagemap *restrict vm, uintptr_t virt, uintptr_t phys, unsigned vprot);
int vmm_patch(struct pagemap *restrict vm, uintptr_t virt, unsigned vprot);
int vmm_unmap(struct pagemap *restrict vm, uintptr_t virt);

void init_vmm(void);

#endif /* INCLUDE_MM_VMM_H */
