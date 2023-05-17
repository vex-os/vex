/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __ARCH_X86_64_INCLUDE_ARCH_INIT_H__
#define __ARCH_X86_64_INCLUDE_ARCH_INIT_H__
#include <arch/limine.h>

extern volatile struct limine_5_level_paging_request five_level_paging_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_kernel_address_request kernel_address_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_terminal_request terminal_request;

#endif /* __ARCH_X86_64_INCLUDE_ARCH_INIT_H__ */
