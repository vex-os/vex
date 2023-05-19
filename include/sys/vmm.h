/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_VMM_H__
#define __INCLUDE_SYS_VMM_H__
#include <stdint.h>
#include <sys/init.h>

#define VPROT_READ  0x0001
#define VPROT_WRITE 0x0002
#define VPROT_EXEC  0x0004
#define VPROT_USER  0x0008

typedef struct pagemap_s {
    void *table;
} pagemap_t;

extern pagemap_t *sys_pagemap;

pagemap_t *vmm_alloc(void);
pagemap_t *vmm_fork(pagemap_t *restrict pagemap);
void vmm_free(pagemap_t *restrict pagemap);
void vmm_switch(pagemap_t *restrict pagemap);
int vmm_map(pagemap_t *restrict pagemap, uintptr_t virt, uintptr_t phys, unsigned short vprot);
int vmm_remap(pagemap_t *restrict pagemap, uintptr_t virt, unsigned short vprot);
int vmm_unmap(pagemap_t *restrict pagemap, uintptr_t virt);

initcall_extern(vmm);

#endif /* __INCLUDE_SYS_VMM_H__ */
