// SPDX-License-Identifier: BSD-3-Clause
#ifndef A9515567_5D7D_43CC_B819_83FC461E4E97
#define A9515567_5D7D_43CC_B819_83FC461E4E97
#pragma once

#include <core/cdefs.h>
#include <stdbool.h>
#include <stdint.h>

#define IDT_SIZE 256

extern void set_idt_entry_kern(unsigned int vector, bool trap, const void* restrict pfn);
extern void set_idt_entry_user(unsigned int vector, bool trap, const void* restrict pfn);
extern void unset_idt_entry(unsigned int vector);

#endif /* A9515567_5D7D_43CC_B819_83FC461E4E97 */
