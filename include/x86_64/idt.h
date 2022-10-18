/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_X86_64_IDT_H__
#define __INCLUDE_X86_64_IDT_H__
#include <kaneru/initcall.h>
#include <stdbool.h>

#define X86_IDT_SIZE 256

void x86_enable_interrupts(void);
void x86_disable_interrupts(void);
bool x86_map_interrupt(int intvec, int idtvec, bool user);

initcall_extern(x86_idt);

#endif /* __INCLUDE_X86_64_IDT_H__ */
