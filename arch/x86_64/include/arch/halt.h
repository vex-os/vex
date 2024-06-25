/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_ARCH_HALT_H
#define INCLUDE_ARCH_HALT_H
#include <core/compiler.h>

static __always_inline inline void halt_cpu(void)
{
    asm volatile("hlt");
}

#endif /* INCLUDE_ARCH_HALT_H */
