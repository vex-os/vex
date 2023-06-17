/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KERN_VMM_H__
#define __INCLUDE_KERN_VMM_H__
#include <kan/cdefs.h>
#include <kern/init.h>
#include <kern/vprot.h>
#include <machine/pml.h>

struct vmm_pagemap {
    uintptr_t vm_ptr;
    pml_t *vm_table;
};

extern struct vmm_pagemap *sys_pagemap;

struct vmm_pagemap *vmm_create(void);
struct vmm_pagemap *vmm_fork(struct vmm_pagemap *restrict stem);
void vmm_destroy(struct vmm_pagemap *restrict pagemap);
void vmm_switch(struct vmm_pagemap *restrict pagemap);
int vmm_map_page(struct vmm_pagemap *restrict pagemap, uintptr_t virt, uintptr_t phys, unsigned short vprot);
int vmm_remap_page(struct vmm_pagemap *restrict pagemap, uintptr_t virt, unsigned short vprot);
int vmm_unmap_page(struct vmm_pagemap *restrict pagemap, uintptr_t virt);

initcall_extern(vmm);

#endif /* __INCLUDE_KERN_VMM_H__ */
