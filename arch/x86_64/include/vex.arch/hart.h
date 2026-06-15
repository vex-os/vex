#ifndef B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#define B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

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

#define HART_FLAG_BSP    0x0001
#define HART_FLAG_ACTIVE 0x0002

struct thread;
struct hart_local {
    unsigned short index;
    unsigned short flags;

    struct thread* curr_thread;
    struct thread* idle_thread;
    unsigned long last_rq_index;

    uint32_t x86_lapic_id;
    uint32_t x86_lapic_resolution;
    struct x86_hart_tss x86_tss;
};

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
    asm volatile("sti");
    return rflags & UINT64_C(0x0000000000000200);
}

static __always_inline bool disable_interrupts(void)
{
    uint64_t rflags;
    asm volatile("pushfq");
    asm volatile("pop %0" : "=r"(rflags));
    asm volatile("cli");
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

struct limine_mp_response;
struct limine_mp_info;

void hart_setup_1(const struct limine_mp_info* restrict info, const struct limine_mp_response* restrict response);
void hart_setup_2(const struct limine_mp_info* restrict info);
void hart_ipi(struct hart_local* restrict target, unsigned ireq_number);

#endif /* B15F19F4_C7E1_479A_A8B7_3E8843A71BA6 */
