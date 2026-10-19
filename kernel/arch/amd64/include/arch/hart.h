#pragma once

#include <arch/amd64.gdt.h>
#include <vex/compiler.h>
#include <vex/tlb.h>
#include <vex/types.h>

struct hart_context {
    uint64_t amd64_ds;
    uint64_t amd64_es;
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

// Entry and exit stubs address the frame by these offsets
static_assert(184 == sizeof(struct hart_context));
static_assert(152 == offsetof(struct hart_context, amd64_cs));

#define syscall_pc     amd64_rip
#define syscall_arg_1  amd64_rdi
#define syscall_arg_2  amd64_rsi
#define syscall_arg_3  amd64_rdx
#define syscall_arg_4  amd64_r10
#define syscall_arg_5  amd64_r8
#define syscall_arg_6  amd64_r9
#define syscall_retval amd64_rax
#define syscall_number amd64_rax

struct amd64_hart_tss {
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

struct hart_local;

struct arch_hart {
    uint32_t lapic_id;
    uint64_t lapic_timer_hz;
    struct amd64_hart_tss tss;
    struct amd64_gdt gdt;
};

static __always_inline void hart_relax(void)
{
    asm volatile("pause");
}

static __always_inline void hart_spin(void)
{
    tlb_poll(); // Answers shootdowns with interrupts disabled

    hart_relax();
}

static __always_inline void hart_stop(void)
{
    asm volatile("hlt");
}

#define AMD64_RFLAGS_FIXED UINT64_C(0x0000000000000002) // Reserved, always set
#define AMD64_RFLAGS_TF    UINT64_C(0x0000000000000100)
#define AMD64_RFLAGS_IF    UINT64_C(0x0000000000000200)
#define AMD64_RFLAGS_DF    UINT64_C(0x0000000000000400)
#define AMD64_RFLAGS_AC    UINT64_C(0x0000000000040000)

static __always_inline __nodiscard uint64_t read_rflags(void)
{
    uint64_t rflags;
    asm volatile("pushfq\n\tpopq %0" : "=r"(rflags));
    return rflags;
}

static __always_inline __nodiscard bool interrupt_state(void)
{
    return read_rflags() & AMD64_RFLAGS_IF;
}

static __always_inline bool enable_interrupts(void)
{
    uint64_t rflags = read_rflags();
    asm volatile("sti" ::: "memory");
    return rflags & AMD64_RFLAGS_IF;
}

static __always_inline bool disable_interrupts(void)
{
    uint64_t rflags = read_rflags();
    asm volatile("cli" ::: "memory");
    return rflags & AMD64_RFLAGS_IF;
}

static __always_inline void restore_interrupts(bool enabled)
{
    if(enabled) {
        asm volatile("sti" ::: "memory");
    }
    else {
        asm volatile("cli" ::: "memory");
    }
}

#define HART_IPI_EVERYONE  ((struct hart_local*)(-1))
#define HART_IPI_EXCEPT_ME ((struct hart_local*)(-2))

void arch_enumerate_harts(void);
void arch_start_hart(struct hart_local* restrict hart);

void hart_setup(struct hart_local* restrict hart);
void hart_ipi(struct hart_local* restrict target, unsigned ireq_number);
