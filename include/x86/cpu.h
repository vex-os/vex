/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef _INCLUDE_X86_CPU_H__
#define _INCLUDE_X86_CPU_H__
#include <kan/compiler.h>
#include <stdint.h>

static __always_inline const uintptr_t *x86_cpu_get_baseptr(void)
{
    const uintptr_t *rbp;
    asm volatile("movq %%rbp, %0":"=r"(rbp)::"memory");
    return rbp;
}

static __always_inline void x86_cpu_disable_interrupts(void)
{
    asm volatile("cli");
}

static __always_inline void x86_cpu_enable_interrupts(void)
{
    asm volatile("sti");
}

static __always_inline void x86_cpu_idle(void)
{
    asm volatile("hlt");
}

#endif /* _INCLUDE_X86_CPU_H__ */
