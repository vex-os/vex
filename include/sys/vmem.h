/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_VMEM_H__
#define __INCLUDE_SYS_VMEM_H__
#include <machine/page.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/initcall.h>

typedef struct vmem_space_s {
    void *vm_table;
} vmem_space_t;

// Kernel address space
extern vmem_space_t *sys_space;

// These symbols are defined by the linker script
extern const char sys_text_start[];
extern const char sys_text_end[];
extern const char sys_rodata_start[];
extern const char sys_rodata_end[];
extern const char sys_data_start[];
extern const char sys_data_end[];
extern const char sys_bss_start[];
extern const char sys_bss_end[];

vmem_space_t *vmem_create(void);
void vmem_delete(vmem_space_t *restrict space);
void vmem_switch(vmem_space_t *restrict space);
int vmem_map_page(vmem_space_t *restrict space, uintptr_t virt, uintptr_t phys, uintmax_t flags);
int vmem_unmap_page(vmem_space_t *restrict space, uintptr_t virt);
uintptr_t vmem_resolve(vmem_space_t *restrict space, uintptr_t virt);

initcall_extern(vmem);

#endif/* __INCLUDE_SYS_VMEM_H__ */
