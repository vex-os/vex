/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_BOOT_H__
#define __INCLUDE_SYS_BOOT_H__
#include <stddef.h>
#include <stdint.h>
#include <sys/initcall.h>

extern const char boot_name[];
extern const char boot_version[];
extern uintptr_t hhdm_offset;
extern uintptr_t load_phys;
extern uintptr_t load_virt;

extern_initcall(boot);

#endif /* __INCLUDE_SYS_BOOT_H__ */
