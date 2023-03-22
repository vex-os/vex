/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_BOOT_H__
#define __INCLUDE_SYS_BOOT_H__
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>

#define BOOT_MEMMAP_RESERV 0x0000
#define BOOT_MEMMAP_KERNEL 0x0001
#define BOOT_MEMMAP_LOADER 0x0002
#define BOOT_MEMMAP_USABLE 0x0003
#define BOOT_MEMMAP_LAST 0xFFFF

typedef struct boot_memmap_s {
    unsigned short type;
    uintptr_t address;
    size_t length;
} boot_memmap_t;

typedef struct boot_module_s {
    const char *path;
    const void *data;
    size_t length;
} boot_module_t;

extern uintptr_t hhdm_offset;
extern uintptr_t load_address_phys;
extern uintptr_t load_address_virt;
extern console_t boot_console;
extern const boot_memmap_t *boot_memmap;
extern const boot_module_t *boot_modules;

initcall_extern(boot);

#endif /* __INCLUDE_SYS_BOOT_H__ */
