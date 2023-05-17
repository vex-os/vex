/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __ARCH_X86_64_INCLUDE_ARCH_CPU_H__
#define __ARCH_X86_64_INCLUDE_ARCH_CPU_H__
#include <kan/cdefs.h>
#include <stdint.h>

static __always_inline inline const uintptr_t *cpu_get_baseptr(void)
{
    const uintptr_t *baseptr;
    asm volatile("movq %%rbp, %0":"=r"(baseptr)::"memory");
    return baseptr;
}

static __always_inline inline void cpu_idle(void)
{
    asm volatile("hlt");
}

static __always_inline inline void cpu_disable_interrupts(void)
{
    asm volatile("cli");
}

static __always_inline inline void cpu_enable_interrupts(void)
{
    asm volatile("sti");
}

static __always_inline inline void cpu_write_cr3(uint64_t value)
{
    asm volatile("movq %0, %%cr3"::"r"(value):"memory");
}

#endif /* __ARCH_X86_64_INCLUDE_ARCH_CPU_H__ */
