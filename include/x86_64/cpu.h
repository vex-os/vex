/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_X86_64_CPU_H__
#define __INCLUDE_X86_64_CPU_H__
#include <stdint.h>
#include <sys/cdefs.h>

typedef struct cpu_ctx_s {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t err, rip, cs, rflags, rsp, ss;
} __packed cpu_ctx_t;

static __always_inline inline const uintptr_t *get_baseptr(void)
{
    const uintptr_t *baseptr;
    asm volatile("movq %%rbp, %0":"=r"(baseptr)::"memory");
    return baseptr;
}

static __always_inline inline void disable_interrupts(void)
{
    asm volatile("cli");
}

static __always_inline inline void enable_interrupts(void)
{
    asm volatile("sti");
}

static __always_inline inline void idle(void)
{
    asm volatile("hlt");
}

#endif /* __INCLUDE_X86_64_CPU_H__ */
