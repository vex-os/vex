/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_X86_64_GDT_H
#define _INCLUDE_X86_64_GDT_H
#include <sys/initcall.h>

#define GDT_NULL 0
#define GDT_KERN_CODE_16 1
#define GDT_KERN_DATA_16 2
#define GDT_KERN_CODE_32 3
#define GDT_KERN_DATA_32 4
#define GDT_KERN_CODE_64 5
#define GDT_KERN_DATA_64 6
#define GDT_USER_CODE_64 7
#define GDT_USER_DATA_64 8
#define GDT_SELECTOR(index, ldt, ring) (((ring) & 4) | (((ldt) & 1) << 2) | (index) << 3)

initcall_extern(x86_gdt);

#endif /* _INCLUDE_X86_64_GDT_H */
