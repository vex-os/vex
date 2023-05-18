/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_X86_64_ARCH_CPU_H__
#define __INCLUDE_X86_64_ARCH_CPU_H__
#include <kan/cdefs.h>
#include <stdint.h>

typedef struct cpu_ctx_s {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rax;
    uint64_t error;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __packed cpu_ctx_t;

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

#endif /* __INCLUDE_X86_64_ARCH_CPU_H__ */
