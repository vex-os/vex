/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_MACHINE_VMEM_H__
#define __INCLUDE_MACHINE_VMEM_H__
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

// Defined by the linker script
// FIXME: move this to a separate header
extern const char sys_text_start[];
extern const char sys_text_end[];
extern const char sys_rodata_start[];
extern const char sys_rodata_end[];
extern const char sys_data_start[];
extern const char sys_data_end[];
extern const char sys_bss_start[];
extern const char sys_bss_end[];

// Kernel's own address space
extern address_space_t *sys_vm;

address_space_t *vmem_alloc(void);
address_space_t *vmem_fork(address_space_t *restrict vm);
void vmem_free(address_space_t *restrict vm);
void vmem_switch_to(address_space_t *restrict vm);
int vmem_map_page(address_space_t *restrict vm, uintptr_t virt, uintptr_t phys, uint64_t flags);
int vmem_map_range(address_space_t *restrict vm, uintptr_t start, uintptr_t end, uintptr_t phys, uint64_t flags);
int vmem_reflag_page(address_space_t *restrict vm, uintptr_t virt, uint64_t flags);
int vmem_unmap_page(address_space_t *restrict vm, uintptr_t virt);
int vmem_unmap_range(address_space_t *restrict vm, uintptr_t start, uintptr_t end);
uintptr_t vmem_resolve(address_space_t *restrict vm, uintptr_t virt);

initcall_extern(vmem);

#endif/* __INCLUDE_MACHINE_VMEM_H__ */
