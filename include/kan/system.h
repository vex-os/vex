/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KAN_SYSTEM_H__
#define __INCLUDE_KAN_SYSTEM_H__
#include <kan/debug.h>
#include <kan/kprintf.h>
#include <stddef.h>
#include <stdint.h>

/* $(BUILD_DIR)/version.c */
extern const char build_date[];
extern const char machine_string[];
extern const char git_revision[];
extern const char version[];

/* $(ARCH_DIR)/ldscript.lds */
extern const char text_start[];
extern const char text_end[];
extern const char rodata_start[];
extern const char rodata_end[];
extern const char data_start[];
extern const char data_end[];
extern const char bss_start[];
extern const char bss_end[];

extern uintptr_t hhdm_offset;
extern uintptr_t kernel_base_phys;
extern uintptr_t kernel_base_virt;

#endif /* __INCLUDE_KAN_SYSTEM_H__ */
