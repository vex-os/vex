/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_UAPI_X86_IDT_H__
#define _INCLUDE_UAPI_X86_IDT_H__
#include <kan/compiler.h>
#include <stdint.h>

/* Maximum IDT size */
#define X86_MAX_INTERRUPTS 256

struct x86_interrupt_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector, error;
    uint64_t rip, cs, rflags;
    uint64_t rsp, ss;
} __packed;

#endif /* _INCLUDE_UAPI_X86_IDT_H__ */
