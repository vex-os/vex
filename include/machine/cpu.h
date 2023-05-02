/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_MACHINE_CPU_H__
#define __INCLUDE_MACHINE_CPU_H__

#if defined(__x86_64__)
#include <x86_64/cpu.h>
#define cpu_ctx_s x86_cpu_ctx_s
#define cpu_ctx_t x86_cpu_ctx_t
#define cpu_get_baseptr() x86_cpu_get_baseptr()
#define cpu_disable_interrupts() x86_cpu_disable_interrupts()
#define cpu_enable_interrupts() x86_cpu_enable_interrupts()
#define cpu_idle() x86_cpu_idle()
#else
#include <stddef.h>
#warning Unknown architecture
typedef struct cpu_ctx_s { int stub; } cpu_ctx_t;
#define cpu_get_baseptr() (NULL)
#define cpu_disable_interrupts() ({})
#define cpu_enable_interrupts() ({})
#define cpu_idle() ({for(;;){continue;}})
#endif

#endif/* __INCLUDE_MACHINE_CPU_H__ */
