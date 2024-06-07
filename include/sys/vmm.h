/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_SYS_VMM_H
#define _INCLUDE_SYS_VMM_H
#include <machine/paging.h>
#include <sys/initcall.h>
#include <sys/vprot.h>

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

initcall_extern(vmm);

#endif /* _INCLUDE_SYS_VMM_H */
