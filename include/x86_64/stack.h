/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_X86_64_STACK_H__
#define __INCLUDE_X86_64_STACK_H__
#include <kaneru/cdefs.h>
#include <stdint.h>

static __always_inline const uintptr_t *x86_get_rbp(void)
{
    const uintptr_t *rbp;
    asm volatile("movq %%rbp, %0":"=r"(rbp)::"memory");
    return rbp;
}

#endif /* __INCLUDE_X86_64_STACK_H__ */
