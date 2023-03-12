/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_X86_64_ASM_H__
#define __INCLUDE_X86_64_ASM_H__

#define ASM_STORE_CTX \
    pushq %rax; pushq %rcx; pushq %rdx; pushq %rbx; pushq %rbp; pushq %rsi; pushq %rdi; \
    pushq %r8; pushq %r9; pushq %r10; pushq %r11; pushq %r12; pushq %r13; pushq %r14; pushq %r15;

#define ASM_RESTORE_CTX \
    popq %r15; popq %r14; popq %r13; popq %r12; popq %r11; popq %r10; popq %r9; popq %r8; \
    popq %rdi; popq %rsi; popq %rbp; popq %rbx; popq %rdx; popq %rcx; popq %rax;

#endif /* __INCLUDE_X86_64_ASM_H__ */
