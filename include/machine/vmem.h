/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_MACHINE_VM_H__
#define __INCLUDE_MACHINE_VM_H__
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/initcall.h>

#if defined(__x86_64__)
#include <stdint.h>
#include <x86_64/vmem.h>
#define VM_WRITEABLE X86_VM_WRITEABLE
#define VM_USERSPACE X86_VM_USERSPACE
#define VM_WRITETHRU X86_VM_WRITETHRU
#define VM_NOEXECUTE X86_VM_NOEXECUTE
#else
#warning Unknown architecture
#define VM_READWRITE 0x00000001
#define VM_USERSPACE 0x00000002
#define VM_WRITETHRU 0x00000004
#define VM_NOEXECUTE 0x00000008
#endif

typedef struct address_space_s {
    void *vm_table;
} address_space_t;

extern address_space_t *sys_space;

address_space_t *vmem_alloc(void);
address_space_t *vmem_copy(address_space_t *restrict space);
void vmem_free(address_space_t *restrict space);
void vmem_switch_to(address_space_t *restrict space);
int vmem_map(address_space_t *restrict space, uintptr_t virt, uintptr_t phys, uintmax_t flags);
int vmem_unmap(address_space_t *restrict space, uintptr_t virt);

initcall_extern(vmem);

#endif/* __INCLUDE_MACHINE_VM_H__ */
