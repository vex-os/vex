/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_BOOT_H__
#define __INCLUDE_SYS_BOOT_H__
#include <stdint.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>

extern console_t boot_console;
extern uintptr_t hhdm_offset;
extern uintptr_t load_phys;
extern uintptr_t load_virt;

initcall_extern(boot);

#endif /* __INCLUDE_SYS_BOOT_H__ */
