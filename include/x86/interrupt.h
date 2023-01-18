#ifndef _INCLUDE_X86_INTERRUPT_H__
#define _INCLUDE_X86_INTERRUPT_H__
#include <kan/initcall.h>
#include <stdint.h>

/* Total size of the IDT */
#define X86_MAX_INTERRUPTS 256

/* FIXME: certain registers are already preserved
 * by the ABI (AMD64 SystemV ABI) so we are possibly
 * doubling them on the stack. Too Bad! */
typedef struct x86_interrupt_frame_s {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t vector, error;
    uint64_t rip, cs, rflags;
    uint64_t rsp, ss;
} __packed x86_interrupt_frame_t;

initcall_extern(x86_interrupt);

#endif /* _INCLUDE_X86_INTERRUPT_H__ */
