/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_TRACE_H__
#define __INCLUDE_KANERU_TRACE_H__
#include <kaneru/cdefs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__X86_64__)
#include <x86_64/stack.h>
#define get_base_pointer() x86_get_rbp()
#else
#define get_base_pointer() ((const uintptr_t *)(NULL))
#endif

struct symbol {
    uintptr_t address;
    const char *name;
};

struct symtab {
    const struct symbol *symbols;
    struct symtab *next;
};

/* defined in kernel/symtab.c */
/* generated via scripts/gen.symtab.sh */
extern const struct symbol __kernel_symbols[];

void register_symtab(struct symtab *restrict st);
bool trace_address(uintptr_t address, struct symbol *restrict sym, uintptr_t *restrict offset);
void print_stack_backtrace(const uintptr_t *base_pointer);

#endif /* __INCLUDE_KANERU_TRACE_H__ */
