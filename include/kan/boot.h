/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _INCLUDE_KAN_BOOT_H__
#define _INCLUDE_KAN_BOOT_H__
#include <limine.h>

extern volatile struct limine_bootloader_info_request boot_info;
extern volatile struct limine_hhdm_request boot_hhdm;
extern volatile struct limine_kernel_address_request boot_kernel_address;
extern volatile struct limine_memmap_request boot_memmap;

#endif /* _INCLUDE_KAN_BOOT_H__ */
