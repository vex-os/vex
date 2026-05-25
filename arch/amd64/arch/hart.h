// SPDX-License-Identifier: BSD-3-Clause
#ifndef B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#define B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#pragma once

#include <core/cdefs.h>

static __always_inline inline void yield_hart(void)
{
    asm volatile("pause");
}

static __always_inline inline void stop_hart(void)
{
    asm volatile("hlt");
}

#endif /* B15F19F4_C7E1_479A_A8B7_3E8843A71BA6 */
