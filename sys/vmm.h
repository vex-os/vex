/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __SYS_VMM_H__
#define __SYS_VMM_H__
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/initcall.h>

#define VMM_READ    0x0001
#define VMM_WRITE   0x0002
#define VMM_EXECUTE 0x0004
#define VMM_USER    0x0008

typedef struct pagemap_s {
    void *table;
} pagemap_t;

extern pagemap_t *kernel_pagemap;

pagemap_t *vmm_create(void);
pagemap_t *vmm_fork(pagemap_t *restrict vm);
void vmm_destroy(pagemap_t *restrict vm);
void vmm_switch_to(pagemap_t *restrict vm);
int vmm_map(pagemap_t *restrict vm, uintptr_t virt, uintptr_t phys, unsigned short mode);
int vmm_unmap(pagemap_t *restrict vm, uintptr_t virt);

initcall_extern(vmm);

#endif /* __SYS_VMM_H__ */
