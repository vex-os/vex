/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_X86_64_INTTAB_H__
#define __INCLUDE_X86_64_INTTAB_H__
#include <sys/initcall.h>

#define X86_IDT_SIZE 256
#define X86_PIC_LINES 32
#define X86_APIC_LINES 256
#define MAX_INTERRUPTS X86_IDT_SIZE
#define MAX_INTERRUPT_REQUESTS X86_APIC_LINES

extern_initcall(inttab);

#endif /* __INCLUDE_X86_64_INTTAB_H__ */
