#ifndef B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#define B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

struct thread;

struct hart_context {
    uint64_t x86_ds;
    uint64_t x86_es;
    uint64_t x86_rax;
    uint64_t x86_rbx;
    uint64_t x86_rcx;
    uint64_t x86_rdx;
    uint64_t x86_rsi;
    uint64_t x86_rdi;
    uint64_t x86_rbp;
    uint64_t x86_r8;
    uint64_t x86_r9;
    uint64_t x86_r10;
    uint64_t x86_r11;
    uint64_t x86_r12;
    uint64_t x86_r13;
    uint64_t x86_r14;
    uint64_t x86_r15;
    uint64_t x86_err;
    uint64_t x86_rip;
    uint64_t x86_cs;
    uint64_t x86_rflags;
    uint64_t x86_rsp;
    uint64_t x86_ss;
} __packed;

struct x86_hart_tss {
    uint32_t reserved_0;
    uint64_t rsp_0;
    uint64_t rsp_1;
    uint64_t rsp_2;
    uint64_t reserved_1;
    uint64_t ist_1;
    uint64_t ist_2;
    uint64_t ist_3;
    uint64_t ist_4;
    uint64_t ist_5;
    uint64_t ist_6;
    uint64_t ist_7;
    uint64_t reserved_2;
    uint32_t iopb;
} __packed;

struct hart_local {
    struct hart_local* self;
    unsigned short index;
    unsigned short flags;
    uint32_t x86_lapic_id;
    uint32_t x86_lapic_reload;
    struct x86_hart_tss x86_tss;
    struct thread* idle_thread;
};

static_assert(0 == offsetof(struct hart_local, self));

static __always_inline __nodiscard struct hart_local* this_hart(void)
{
    struct hart_local* hart;
    asm volatile("movq %%gs:0, %0" : "=r"(hart));
    return hart;
}

static __always_inline void hart_spin(void)
{
    asm volatile("pause");
}

static __always_inline void hart_stop(void)
{
    asm volatile("hlt");
}

static __always_inline __nodiscard bool interrupt_state(void)
{
    uint64_t rflags;
    asm volatile("pushfq");
    asm volatile("pop %0" : "=r"(rflags));
    return rflags & UINT64_C(0x0000000000000200);
}

static __always_inline bool enable_interrupts(void)
{
    uint64_t rflags;
    asm volatile("pushfq");
    asm volatile("pop %0" : "=r"(rflags));
    asm volatile("cli");
    return rflags & UINT64_C(0x0000000000000200);
}

static __always_inline bool disable_interrupts(void)
{
    uint64_t rflags;
    asm volatile("pushfq");
    asm volatile("pop %0" : "=r"(rflags));
    asm volatile("sti");
    return rflags & UINT64_C(0x0000000000000200);
}

static __always_inline void restore_interrupts(bool enabled)
{
    if(enabled) {
        asm volatile("sti");
    }
    else {
        asm volatile("cli");
    }
}

void broadcast_panic_ipi(void);

#endif /* B15F19F4_C7E1_479A_A8B7_3E8843A71BA6 */
