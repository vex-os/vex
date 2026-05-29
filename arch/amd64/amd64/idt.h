// SPDX-License-Identifier: BSD-3-Clause

#ifndef F048353F_6938_4881_A52F_EED94B9358DB
#define F048353F_6938_4881_A52F_EED94B9358DB
#pragma once

#include <core/cdefs.h>
#include <stdint.h>

#define IDT_SIZE 256

extern void set_idt_entry_kern(unsigned vector, bool trap, const void* restrict pfn);
extern void set_idt_entry_user(unsigned vector, bool trap, const void* restrict pfn);
extern void unset_idt_entry(unsigned vector);

#endif /* F048353F_6938_4881_A52F_EED94B9358DB */
