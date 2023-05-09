/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __SYS_SYSTM_H__
#define __SYS_SYSTM_H__
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

#define BOOTFLAG_VMM_PML3 0x0001
#define BOOTFLAG_VMM_PML4 0x0002
#define BOOTFLAG_VMM_PML5 0x0004

#define MEMMAP_USABLE       0x0000 /* x86_64 BIOS 0xE820 */
#define MEMMAP_RESERVED     0x0001 /* x86_64 BIOS 0xE820 */
#define MEMMAP_ACPI_DATA    0x0002 /* x86_64 BIOS 0xE820 */
#define MEMMAP_ACPI_NVS     0x0003 /* x86_64 BIOS 0xE820 */
#define MEMMAP_BAD_MEMORY   0x0004 /* x86_64 BIOS 0xE820 */
#define MEMMAP_BOOTLOADER   0x0005 /* Limine protocol */
#define MEMMAP_KERNEL       0x0006 /* Limine protocol */
#define MEMMAP_FRAMEBUFFER  0x0007 /* Limine protocol */

typedef struct memmap_s {
    unsigned short type;
    uintptr_t address;
    size_t length;
} memmap_t;

/* $(TEMP_DIR)/version.c */
extern const char build_date[];
extern const char machine_string[];
extern const char git_revision[];
extern const char version[];

extern const char text_start[];
extern const char text_end[];
extern const char rodata_start[];
extern const char rodata_end[];
extern const char data_start[];
extern const char data_end[];
extern const char bss_start[];
extern const char bss_end[];

extern memmap_t *memmap;
extern size_t memmap_size;
extern uintptr_t highmem_offset;
extern uintptr_t kernel_base_phys;
extern uintptr_t kernel_base_virt;
extern unsigned short bootflags;

void kputchar(int c);
void kprintf(const char *restrict fmt, ...) __printflike(1, 2);
void kvprintf(const char *restrict fmt, va_list ap) __printflike(1, 0);

void fpanic(const char *restrict file, unsigned long line, const char *restrict fmt, ...) __noreturn __printflike(3, 4);
void fvpanic(const char *restrict file, unsigned long line, const char *restrict fmt, va_list ap) __noreturn __printflike(3, 0);
#define panic(fmt, ...) fpanic((__FILE__), (__LINE__), (fmt), ##__VA_ARGS__)
#define vpanic(fmt, ap) fvpanic((__FILE__), (__LINE__), (fmt), (ap))

#define kassert(x) ({if_unlikely(!(x)){panic("assertion failed: %s", #x);UNREACHABLE();}})
#define kassertf(x, fmt, ...) ({if_unlikely(!(x)){panic((fmt), ##__VA_ARGS__);UNREACHABLE();}})
#define panic_if(x, fmt, ...) ((if_unlikely(x){panic((fmt), ##__VA_ARGS__);UNREACHABLE();}))

#endif /* __SYS_SYSTM_H__ */
