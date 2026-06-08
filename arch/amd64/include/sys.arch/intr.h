#ifndef FD177992_9E0F_4161_81C4_364624A1E1F6
#define FD177992_9E0F_4161_81C4_364624A1E1F6
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

struct interrupt_frame {
    uint64_t amd64_rax;
    uint64_t amd64_rbx;
    uint64_t amd64_rcx;
    uint64_t amd64_rdx;
    uint64_t amd64_rsi;
    uint64_t amd64_rdi;
    uint64_t amd64_rbp;
    uint64_t amd64_r8;
    uint64_t amd64_r9;
    uint64_t amd64_r10;
    uint64_t amd64_r11;
    uint64_t amd64_r12;
    uint64_t amd64_r13;
    uint64_t amd64_r14;
    uint64_t amd64_r15;
    uint64_t amd64_err;
    uint64_t amd64_rip;
    uint64_t amd64_cs;
    uint64_t amd64_rflags;
    uint64_t amd64_rsp;
    uint64_t amd64_ss;
} __packed;

static __always_inline bool disable_interrupts(void)
{
    uint64_t rflags;
    asm volatile("pushfq");
    asm volatile("pop %0" : "=r"(rflags));
    asm volatile("cli");
    return rflags & UINT64_C(0x0000000000000200);
}

static __always_inline bool enable_interrupts(void)
{
    uint64_t rflags;
    asm volatile("pushfq");
    asm volatile("pop %0" : "=r"(rflags));
    asm volatile("sti");
    return rflags & UINT64_C(0x0000000000000200);
}

static __always_inline void restore_interrupts(bool state)
{
    if(state) {
        asm volatile("sti");
    }
    else {
        asm volatile("cli");
    }
}

#endif /* FD177992_9E0F_4161_81C4_364624A1E1F6 */
