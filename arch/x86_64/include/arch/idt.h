/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_ARCH_IDT_H
#define INCLUDE_ARCH_IDT_H
#include <core/compiler.h>
#include <stdint.h>

void set_idt_entry(unsigned vector, int trap, const void *restrict pfn);
void set_idt_entry_user(unsigned vector, int trap, const void *restrict pfn);
void unset_idt_entry(unsigned vector);

void init_idt(void);

#endif /* INCLUDE_ARCH_IDT_H */
