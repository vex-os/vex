#ifndef C1107E57_2AC9_4B7E_8B6C_67E3F9AC096F
#define C1107E57_2AC9_4B7E_8B6C_67E3F9AC096F

#include <core/cdefs.h>

#include <stddef.h>
#include <stdint.h>

struct arch_interrupt_frame {
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

static __always_inline inline void arch_disable_interrupts(void)
{
    asm volatile("cli");
}

static __always_inline inline void arch_enable_interrupts(void)
{
    asm volatile("sti");
}

#endif /* C1107E57_2AC9_4B7E_8B6C_67E3F9AC096F */
