/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __SYS_VMM_H__
#define __SYS_VMM_H__
#include <machine/paging.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/initcall.h>

typedef struct vma_s {
    pml_t *table;
} vma_t;

extern vma_t *kernel_vma;

vma_t *vmm_create(void);
vma_t *vmm_fork(vma_t *restrict vma);
void vmm_destroy(vma_t *restrict vma);
void vmm_switch_to(vma_t *restrict vma);
int vmm_map(vma_t *restrict vma, uintptr_t virt, uintptr_t phys, vmode_t mode);
int vmm_mode(vma_t *restrict vma, uintptr_t virt, vmode_t mode);
int vmm_unmap(vma_t *restrict vma, uintptr_t virt);

initcall_extern(vmm);

#endif /* __SYS_VMM_H__ */
