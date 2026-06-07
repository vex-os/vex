#ifndef D718BA46_38D6_497E_BAA7_5BE2DEA1BC9D
#define D718BA46_38D6_497E_BAA7_5BE2DEA1BC9D
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

void amd64_task_trampoline(void);

static __always_inline __nodiscard uintptr_t* setup_task_stack(uintptr_t* restrict stack_ptr, void* restrict entry_addr,
    void* restrict ctxdata)
{
    stack_ptr[-1] = (uintptr_t)(&amd64_task_trampoline); // ret --> trampoline
    stack_ptr[-2] = 0;                                   // rbp
    stack_ptr[-3] = 0;                                   // rbx
    stack_ptr[-4] = (uintptr_t)(entry_addr);             // r12 = entry
    stack_ptr[-5] = (uintptr_t)(ctxdata);                // r13 = ctxdata
    stack_ptr[-6] = 0;                                   // r14
    stack_ptr[-7] = 0;                                   // r15  <-- task->kstack points here
    return stack_ptr - 7;
}

#endif /* D718BA46_38D6_497E_BAA7_5BE2DEA1BC9D */
