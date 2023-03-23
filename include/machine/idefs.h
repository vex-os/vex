/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_MACHINE_IDEFS_H__
#define __INCLUDE_MACHINE_IDEFS_H__

#if defined(__x86_64__)
#include <x86_64/idefs.h>
#define MAX_INTERRUPTS X86_IDT_SIZE
#define MAX_INTERRUPT_REQUESTS X86_APIC_LINES
#else
#warning Unknown architecture
#define MAX_INTERRUPTS 1
#define MAX_INTERRUPT_REQUESTS 1
#endif

#endif/* __INCLUDE_MACHINE_IDEFS_H__ */
