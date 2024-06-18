/* SPDX-License-Identifier: Zlib */
#ifndef ARCH_X86_64_INCLUDE_ARCH_CPU_H
#define ARCH_X86_64_INCLUDE_ARCH_CPU_H
#include <iserix/compiler.h>
#include <stdint.h>

static ALWAYS_INLINE inline void halt_cpu(void)
{
    asm volatile("hlt");
}

static ALWAYS_INLINE inline void disable_interrupts(void)
{
    asm volatile("cli");
}

static ALWAYS_INLINE inline void enable_interrupts(void)
{
    asm volatile("sti");
}

static ALWAYS_INLINE inline void set_cpu_pagemap(uint64_t phys)
{
    asm volatile("movq %0, %%cr3"::"r"(phys):"memory");
}

#endif /* ARCH_X86_64_INCLUDE_ARCH_CPU_H */
