/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_X86_64_CPU_H
#define _INCLUDE_X86_64_CPU_H
#include <stdint.h>
#include <sys/cdefs.h>

static __always_inline inline void disable_interrupts(void)
{
    asm volatile("cli");
}

static __always_inline inline void enable_interrupts(void)
{
    asm volatile("sti");
}

static __always_inline inline void halt_cpu(void)
{
    asm volatile("hlt");
}

static __always_inline inline void set_cpu_pagemap(uint64_t phys)
{
    asm volatile("movq %0, %%cr3"::"r"(phys):"memory");
}

#endif /* _INCLUDE_X86_64_CPU_H */
