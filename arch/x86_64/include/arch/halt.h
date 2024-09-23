/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef INCLUDE_ARCH_HALT_H
#define INCLUDE_ARCH_HALT_H
#include <kern/compiler.h>

static __always_inline inline void halt_cpu(void)
{
    asm volatile("hlt");
}

#endif /* INCLUDE_ARCH_HALT_H */
