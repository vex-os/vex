#ifndef D718BA46_38D6_497E_BAA7_5BE2DEA1BC9D
#define D718BA46_38D6_497E_BAA7_5BE2DEA1BC9D
#pragma once

#include <sys.amd64/gdt.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#define AMD64_RFLAGS_IF (1 << 9)

extern void amd64_kthread_trampoline(void);

static __always_inline __nodiscard uintptr_t* setup_kthread_kstack(uintptr_t* restrict stack_ptr, void* restrict entry_addr,
    void* restrict ctxdata)
{
    stack_ptr[-1] = (uintptr_t)(&amd64_kthread_trampoline); // ret --> trampoline
    stack_ptr[-2] = 0;                                      // rbp
    stack_ptr[-3] = 0;                                      // rbx
    stack_ptr[-4] = (uintptr_t)(entry_addr);                // r12 = entry
    stack_ptr[-5] = (uintptr_t)(ctxdata);                   // r13 = ctxdata
    stack_ptr[-6] = 0;                                      // r14
    stack_ptr[-7] = 0;                                      // r15  <-- kthread->kstack points here
    return stack_ptr - 7;
}

static __always_inline __nodiscard uintptr_t* setup_uthread_kstack(uintptr_t* restrict stack_ptr, uintptr_t addr, uintptr_t ustack)
{
    stack_ptr[-1] = gdt_selector(GDT_USER_DATA_64, 0, 3); // ss
    stack_ptr[-2] = ustack;                               // rsp (user stack)
    stack_ptr[-3] = AMD64_RFLAGS_IF;                      // rflags
    stack_ptr[-4] = gdt_selector(GDT_USER_CODE_64, 0, 3); // cs
    stack_ptr[-5] = addr;                                 // rip <-- uthread->kstack points here
    return stack_ptr - 5;
}

#endif /* D718BA46_38D6_497E_BAA7_5BE2DEA1BC9D */
