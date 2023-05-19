/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_X86_64_MACHINE_BOOT_H__
#define __INCLUDE_X86_64_MACHINE_BOOT_H__
#include <limine.h>

extern volatile struct limine_5_level_paging_request five_level_paging_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_kernel_address_request kernel_address_request;
extern volatile struct limine_memmap_request memmap_request;

extern uintptr_t hhdm_offset;
extern uintptr_t kernel_base_virt;
extern uintptr_t kernel_base_phys;

#endif /* __INCLUDE_X86_64_MACHINE_BOOT_H__ */
