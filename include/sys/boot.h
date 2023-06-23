/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_BOOT_H__
#define __INCLUDE_SYS_BOOT_H__
#include <limine.h>
#include <stdint.h>

extern uintptr_t hhdm_offset;
extern uintptr_t kernel_base_phys;
extern uintptr_t kernel_base_virt;

extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_kernel_address_request kernel_address_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_framebuffer_request framebuffer_request;

#endif /* __INCLUDE_SYS_BOOT_H__ */
