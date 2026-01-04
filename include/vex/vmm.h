#ifndef _VEX_VMM_H
#define _VEX_VMM_H 1

#include <arch/paging.h>

#include <vex/compiler.h>
#include <vex/vprot.h>

struct pagemap {
    pmentry_t* virt;
    uintptr_t phys;
};

extern struct pagemap sys_pagemap;

extern struct pagemap* vmm_create(void);
extern struct pagemap* vmm_fork(struct pagemap* restrict stem);
extern void vmm_destroy(struct pagemap* restrict vm);
extern void vmm_switch(struct pagemap* restrict vm);

extern int vmm_map(struct pagemap* restrict vm, uintptr_t virt, uintptr_t phys, unsigned int vprot);
extern int vmm_patch(struct pagemap* restrict vm, uintptr_t virt, unsigned int vprot);
extern int vmm_unmap(struct pagemap* restrict vm, uintptr_t virt);

extern uintptr_t vmm_resolve(struct pagemap* restrict vm, uintptr_t virt);
extern uintptr_t vmm_resolve_ptr(struct pagemap* restrict vm, const void* restrict ptr);

extern void init_vmm(void);

#endif
