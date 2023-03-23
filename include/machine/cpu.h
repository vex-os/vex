/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_MACHINE_CPU_H__
#define __INCLUDE_MACHINE_CPU_H__

#if defined(__x86_64__)
#include <x86_64/cpu.h>
#define cpu_ctx_s x86_cpu_ctx_s
#define cpu_ctx_t x86_cpu_ctx_t
#define get_baseptr() x86_get_baseptr()
#define disable_interrupts() x86_disable_interrupts()
#define enable_interrupts() x86_enable_interrupts()
#define idle() x86_idle()
#else
#warning Unknown architecture
typedef struct cpu_ctx_s { int stub; } cpu_ctx_t;
#define get_baseptr() ({})
#define disable_interrupts() ({})
#define enable_interrupts() ({})
#define idle() ({for(;;){continue;}})
#endif

#endif/* __INCLUDE_MACHINE_CPU_H__ */
