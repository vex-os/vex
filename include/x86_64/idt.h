/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_X86_64_IDT_H__
#define __INCLUDE_X86_64_IDT_H__
#include <kaneru/cdefs.h>
#include <kaneru/initcall.h>
#include <kaneru/intvec.h>
#include <stdbool.h>
#include <stdint.h>

#define X86_IDT_SIZE 256

struct x86_interrupt_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector, error;
    uint64_t rip, cs, rflags;
    uint64_t rsp, ss;
} __packed;

void x86_enable_interrupts(void);
void x86_disable_interrupts(void);
bool x86_map_interrupt(intvec_t intvec, unsigned int vector, bool user);

initcall_extern(x86_idt);

#endif /* __INCLUDE_X86_64_IDT_H__ */
