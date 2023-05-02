/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_MACHINE_BOOT_H__
#define __INCLUDE_MACHINE_BOOT_H__
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/initcall.h>

#define MEMMAP_USABLE       0x0000  // x86_64 BIOS 0xE820
#define MEMMAP_RESERVED     0x0001  // x86_64 BIOS 0xE820
#define MEMMAP_ACPI_DATA    0x0002  // x86_64 BIOS 0xE820
#define MEMMAP_ACPI_NVS     0x0003  // x86_64 BIOS 0xE820
#define MEMMAP_BAD_MEMORY   0x0004  // x86_64 BIOS 0xE820
#define MEMMAP_BOOTLOADER   0x0005  // Limine protocol
#define MEMMAP_KERNEL       0x0006  // Limine protocol
#define MEMMAP_FRAMEBUFFER  0x0007  // Limine protocol

#define BOOTOPT_X86_PML5    0x0001  // X86.PML5 virtual memory
#define BOOTOPT_RV64_SV5    0x0001  // RV64.SV5 virtual memory

// UNDONE: iterate_framebuffers_t
typedef void (*iterate_memmap_t)(uintptr_t address, size_t n, unsigned short type, void *restrict arg);
typedef void (*iterate_modules_t)(const char *restrict path, const void *restrict s, size_t n, void *restrict arg);

extern uintptr_t hhdm_offset;
extern uintptr_t kernel_base_phys;
extern uintptr_t kernel_base_virt;
extern unsigned short bootopts;

// When the system transitions to userspace stage,
// the memory occupied by the bootloader is reclaimed
// via iterate_memmap() and then this function is called,
// invalidating any non-portable bootloader data, and by
// consequence, making future iterate_xxx() calls invalid.
void invalidate_boot_structures(void);

// UNDONE: iterate_framebuffers()
int iterate_memmap(iterate_memmap_t callback, void *restrict arg);
int iterate_modules(iterate_modules_t callback, void *restrict arg);

initcall_extern(boot);

#endif/* __INCLUDE_MACHINE_BOOT_H__ */
