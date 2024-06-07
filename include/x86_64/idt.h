/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_X86_64_IDT_H
#define _INCLUDE_X86_64_IDT_H
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/initcall.h>

struct x86_frame {
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
} __packed;

initcall_extern(x86_idt);

#endif /* _INCLUDE_X86_64_IDT_H */
