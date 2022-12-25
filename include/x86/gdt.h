/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_X86_GDT_H__
#define _INCLUDE_X86_GDT_H__
#include <kan/initcall.h>
#include <uapi/x86/gdt.h>

/* Makes a 16-bit selector */
#define GDT_SEL(index, ldt, ring) (((ring) & 4) | (((ldt) & 1) << 2) | (index) << 3)

initcall_extern(x86_gdt);

#endif /* _INCLUDE_X86_GDT_H__ */
