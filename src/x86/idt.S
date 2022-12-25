/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */

.section .text
.global __x86_interrupt_handler_asm
__x86_interrupt_handler_asm:
    /* x86_64 appears to not have any
     * instructions that can push all the
     * registers within our interest circle
     * to the stack. So we have to improvise. */
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* Call a C function:
     *  void __x86_interrupt_handler(x86_interrupt_frame ptr) */
    movq %rsp, %rdi
    call __x86_interrupt_handler

    /* Discard the interrupt frame from the stack.
     * NOTE: as a side effect, anything done with
     * the frame's values affects real registers */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    /* Drop frame values saved
     * by an auto-generated ISRs */
    addq $0x10, %rsp

    sti
    iretq
