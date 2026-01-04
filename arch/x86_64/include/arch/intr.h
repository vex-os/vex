#ifndef _ARCH_INTR_H
#define _ARCH_INTR_H 1

#include <vex/compiler.h>

#include <stddef.h>
#include <stdint.h>

#define MAX_INTERRUPT_VECTORS 256

struct interrupt_frame {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t error;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __packed;

static __always_inline inline void disable_interrupts(void)
{
    asm volatile("cli");
}

static __always_inline inline void enable_interrupts(void)
{
    asm volatile("sti");
}

#endif
