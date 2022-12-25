/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_KAN_CPU_H__
#define _INCLUDE_KAN_CPU_H__
#include <stddef.h>
#include <stdint.h>

#if defined(__X86_64__)
#include <x86/cpu.h>
#define cpu_get_baseptr() x86_cpu_get_baseptr()
#define cpu_disable_interrupts() x86_cpu_disable_interrupts()
#define cpu_enable_interrupts() x86_cpu_enable_interrupts()
#define cpu_idle() x86_cpu_idle()
#else
#define cpu_get_baseptr() ((const uintptr *)(NULL))
#define cpu_disable_interrupts() ({})
#define cpu_enable_interrupts() ({})
#define cpu_idle() ({for(;;);})
#endif

#endif /* _INCLUDE_KAN_CPU_H__ */
