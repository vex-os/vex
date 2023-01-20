/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _INCLUDE_KAN_BOOTINFO_H__
#define _INCLUDE_KAN_BOOTINFO_H__
#include <kan/initcall.h>
#include <limine.h>
#include <stddef.h>
#include <stdint.h>

uintptr_t get_hhdm_offset(void);
uintptr_t get_kernel_address_phys(void);
uintptr_t get_kernel_address_virt(void);
const struct limine_memmap_response *get_memmap(void);
const struct limine_module_response *get_modules(void);
const struct limine_framebuffer_response *get_framebuffers(void);

initcall_extern(bootinfo);

#endif /* _INCLUDE_KAN_BOOTINFO_H__ */
